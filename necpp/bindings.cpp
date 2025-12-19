#include <emscripten/bind.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <complex>

#include "src/nec_context.h"
#include "src/nec_exception.h"
#include "src/c_geometry.h"

using namespace emscripten;
using namespace std;

struct JsComplex {
    double r, i;
};

struct SimulationSummary {
    double freq_mhz;
    JsComplex impedance;
    double swr_50;
    bool success;
    string error;
    string debug_info; 
};

struct SegmentCurrent {
    int tag_id;
    int segment_no;
    double start_x, start_y, start_z;
    double end_x, end_y, end_z;
    double mag;
    double phase;
    double real;
    double imag;
};

struct RadiationPoint {
    double theta, phi, v_gain, h_gain, total_gain;
};

class NecWrapper {
private:
    nec_context nec;
    double current_freq;

public:
    NecWrapper() : current_freq(0.0) {
        nec.initialize();
    }

    void setFrequency(double freq_mhz) {
        current_freq = freq_mhz;
        nec.fr_card(0, 1, freq_mhz, 0.0);
    }

    void setExtendedKernel(bool enable) {
        nec.set_extended_thin_wire_kernel(enable);
    }

    void addWire(int tag, int segs, double x1, double y1, double z1, double x2, double y2, double z2, double rad) {
        nec.wire(tag, segs, x1, y1, z1, x2, y2, z2, rad, 1.0, 1.0);
    }

    void geometryComplete(int ground_type) {
        nec.geometry_complete(ground_type);
    }

    void addVoltageSource(int tag, int seg, double r, double i) {
        nec.ex_card(EXCITATION_VOLTAGE, tag, seg, 0, r, i, 0, 0, 0, 0);
    }

    string getGeometryDebugInfo() {
        stringstream ss;
        c_geometry* geo = nec.get_geometry();
        int n = geo->n_segments;
        
        ss << "=== NEC GEOMETRY DEBUG REPORT ===\n";
        ss << "Total Segments: " << n << "\n";
        ss << "Freq: " << current_freq << " MHz\n";
        ss << fixed << setprecision(4);
        
        ss << "----------------------------------------------------------------\n";
        ss << "Seg | Tag | Start(X,Y,Z)       | End(X,Y,Z)         | Conn1 | Conn2 \n";
        ss << "----------------------------------------------------------------\n";
        
        for (int i = 0; i < n; i++) {
            int conn1 = geo->icon1[i];
            int conn2 = geo->icon2[i];
            double len = geo->segment_length[i];
            double x1 = geo->x[i] - 0.5 * len * geo->cab[i];
            double y1 = geo->y[i] - 0.5 * len * geo->sab[i];
            double z1 = geo->z[i] - 0.5 * len * geo->salp[i];
            double x2 = geo->x[i] + 0.5 * len * geo->cab[i];
            double y2 = geo->y[i] + 0.5 * len * geo->sab[i];
            double z2 = geo->z[i] + 0.5 * len * geo->salp[i];

            ss << setw(3) << (i+1) << " | " 
               << setw(3) << geo->segment_tags[i] << " | "
               << "(" << setw(6) << x1 << "," << setw(6) << y1 << "," << setw(6) << z1 << ") | "
               << "(" << setw(6) << x2 << "," << setw(6) << y2 << "," << setw(6) << z2 << ") | "
               << setw(5) << conn1 << " | " << setw(5) << conn2 << "\n";
        }
        ss << "----------------------------------------------------------------\n";
        return ss.str();
    }

    SimulationSummary runSimulation() {
        SimulationSummary res;
        res.success = true;
        res.freq_mhz = current_freq;

        try {
            res.debug_info = getGeometryDebugInfo();
            nec.xq_card(0); 

            nec_antenna_input* input_data = nec.get_input_parameters(0);
            if (input_data) {
                vector<nec_complex>& z_vec = input_data->get_impedance();
                if (!z_vec.empty()) {
                    nec_complex z = z_vec.back(); 
                    res.impedance.r = z.real();
                    res.impedance.i = z.imag();

                    double z0 = 50.0;
                    complex<double> z_c(z.real(), z.imag());
                    complex<double> rho = (z_c - z0) / (z_c + z0);
                    double rho_mag = abs(rho);
                    if (rho_mag >= 1.0) res.swr_50 = 999.0;
                    else res.swr_50 = (1.0 + rho_mag) / (1.0 - rho_mag);
                }
            } else {
                res.impedance = {0,0}; res.swr_50 = 0;
            }

        } catch (const char* msg) {
            res.success = false; res.error = msg;
        } catch (nec_exception* e) {
            res.success = false; res.error = e->get_message();
        } catch (...) {
            res.success = false; res.error = "Unknown Error";
        }
        return res;
    }

    vector<SegmentCurrent> getCurrents() {
        vector<SegmentCurrent> ret;
        c_geometry* geo = nec.get_geometry();
        int n_segs = geo->n_segments;

        for (int i = 0; i < n_segs; i++) {
            SegmentCurrent sc;
            sc.segment_no = i + 1;
            sc.tag_id = geo->segment_tags[i];
            double len = geo->segment_length[i];
            sc.start_x = geo->x[i] - 0.5 * len * geo->cab[i];
            sc.start_y = geo->y[i] - 0.5 * len * geo->sab[i];
            sc.start_z = geo->z[i] - 0.5 * len * geo->salp[i];
            sc.end_x   = geo->x[i] + 0.5 * len * geo->cab[i];
            sc.end_y   = geo->y[i] + 0.5 * len * geo->sab[i];
            sc.end_z   = geo->z[i] + 0.5 * len * geo->salp[i];

            if (i < nec.current_vector.size()) {
                nec_complex c = nec.current_vector[i]; 
                sc.real = c.real();
                sc.imag = c.imag();
                sc.mag  = abs(c);
                sc.phase = std::arg(c) * 180.0 / 3.14159265359;
            }
            ret.push_back(sc);
        }
        return ret;
    }

    vector<RadiationPoint> calculatePattern(int theta_steps, int phi_steps) {
        vector<RadiationPoint> ret;
        double d_theta = 180.0 / (theta_steps > 1 ? theta_steps - 1 : 1);
        double d_phi   = 360.0 / (phi_steps > 1 ? phi_steps - 1 : 1);
        double wavelength = 299.792458 / current_freq;

        for (int i = 0; i < theta_steps; i++) {
            double theta_deg = i * d_theta;
            double theta_rad = theta_deg * 3.14159265359 / 180.0;
            for (int j = 0; j < phi_steps; j++) {
                double phi_deg = j * d_phi;
                double phi_rad = phi_deg * 3.14159265359 / 180.0;
                nec_complex eth, eph;
                nec.ffld(theta_rad, phi_rad, &eth, &eph, wavelength);
                double v_gain = norm(eth); 
                double h_gain = norm(eph); 
                double t_gain = v_gain + h_gain;
                RadiationPoint p;
                p.theta = theta_deg; p.phi = phi_deg;
                p.v_gain = 10 * log10(v_gain + 1e-20);
                p.h_gain = 10 * log10(h_gain + 1e-20);
                p.total_gain = 10 * log10(t_gain + 1e-20);
                ret.push_back(p);
            }
        }
        return ret;
    }
};

EMSCRIPTEN_BINDINGS(nec_module) {
    register_vector<SegmentCurrent>("VectorSegmentCurrent");
    register_vector<RadiationPoint>("VectorRadiationPoint");

    value_object<JsComplex>("JsComplex")
        .field("r", &JsComplex::r)
        .field("i", &JsComplex::i);

    value_object<SimulationSummary>("SimulationSummary")
        .field("freq_mhz", &SimulationSummary::freq_mhz)
        .field("impedance", &SimulationSummary::impedance)
        .field("swr_50", &SimulationSummary::swr_50)
        .field("success", &SimulationSummary::success)
        .field("error", &SimulationSummary::error)
        .field("debug_info", &SimulationSummary::debug_info);

    value_object<SegmentCurrent>("SegmentCurrent")
        .field("tag_id", &SegmentCurrent::tag_id)
        .field("segment_no", &SegmentCurrent::segment_no)
        .field("start_x", &SegmentCurrent::start_x)
        .field("start_y", &SegmentCurrent::start_y)
        .field("start_z", &SegmentCurrent::start_z)
        .field("end_x", &SegmentCurrent::end_x)
        .field("end_y", &SegmentCurrent::end_y)
        .field("end_z", &SegmentCurrent::end_z)
        .field("mag", &SegmentCurrent::mag)
        .field("phase", &SegmentCurrent::phase)
        .field("real", &SegmentCurrent::real)
        .field("imag", &SegmentCurrent::imag);

    value_object<RadiationPoint>("RadiationPoint")
        .field("theta", &RadiationPoint::theta)
        .field("phi", &RadiationPoint::phi)
        .field("v_gain", &RadiationPoint::v_gain)
        .field("h_gain", &RadiationPoint::h_gain)
        .field("total_gain", &RadiationPoint::total_gain);

    class_<NecWrapper>("NecEngine")
        .constructor<>()
        .function("setFrequency", &NecWrapper::setFrequency)
        .function("setExtendedKernel", &NecWrapper::setExtendedKernel)
        .function("addWire", &NecWrapper::addWire)
        .function("geometryComplete", &NecWrapper::geometryComplete)
        .function("addVoltageSource", &NecWrapper::addVoltageSource)
        .function("getGeometryDebugInfo", &NecWrapper::getGeometryDebugInfo)
        .function("runSimulation", &NecWrapper::runSimulation)
        .function("getCurrents", &NecWrapper::getCurrents)
        .function("calculatePattern", &NecWrapper::calculatePattern);
}