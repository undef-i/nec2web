<script setup>
import { ref, reactive, onMounted, onBeforeUnmount, watch, nextTick } from "vue";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import AnalysisCharts from "./AnalysisCharts.vue";
import { presets } from "./presets.js";

const wasmModule = ref(null);
const canvasRef = ref(null);

const status = reactive({
  ready: false,
  calculating: false,
  progress: 0,
  msg: "Initializing...",
});

const ui = reactive({
  sidebarOpen: true,
  tab: "single",
  scale: 0.1,
  showCurrents: true,
  showPattern: true,
});

const params = reactive({
  freq: 145.0,
  wires: [],
  source: { tag: 1, seg: 1, real: 1.0, imag: 0.0 },
});

const sweepParams = reactive({ start: 100, end: 500, steps: 40 });

const result = reactive({
  hasResult: false,
  freq: 0,
  swr: 0,
  gain: 0,
  z_r: 0,
  z_i: 0,
  currents: [],
  pattern: [],
});

const sweepData = reactive({ labels: [], values: [], impedance: [] });

onMounted(() => {
  const script = document.createElement("script");
  script.src = "/nec_engine.js";
  script.onload = async () => {
    try {
      wasmModule.value = await window.createNecEngine();
      status.ready = true;
      status.msg = "Ready";
      initThree();
    } catch (e) {
      status.msg = "WASM Error: " + e;
    }
  };
  document.head.appendChild(script);
});

onBeforeUnmount(() => {
  cancelAnimationFrame(animationId);
  if (renderer) renderer.dispose();
});

const loadPreset = async (p) => {
  if (!p) return;
  Object.assign(params, JSON.parse(JSON.stringify(p)));
  result.hasResult = false;
  result.currents = [];
  result.pattern = [];
  sweepParams.start = Math.floor(p.freq * 0.9);
  sweepParams.end = Math.ceil(p.freq * 1.1);

  await nextTick();
  update3DScene();
  fitCameraToObject();
};

const createEngine = () => {
  const eng = new wasmModule.value.NecEngine();
  if (params.wires.length) {
    params.wires.forEach((w, i) => {
      eng.addWire(
        i + 1,
        Number(w.segs),
        Number(w.x1),
        Number(w.y1),
        Number(w.z1),
        Number(w.x2),
        Number(w.y2),
        Number(w.z2),
        Number(w.rad)
      );
    });
    eng.geometryComplete(0);
    if (params.wires.length >= params.source.tag) {
      eng.addVoltageSource(
        Number(params.source.tag),
        Number(params.source.seg),
        Number(params.source.real),
        Number(params.source.imag)
      );
    }
    if (eng.setExtendedKernel) eng.setExtendedKernel(true);
  }
  return eng;
};

const extractCurrentsToJS = (eng) => {
  try {
    const vec = eng.getCurrents();
    const arr = [];
    const size = vec.size();
    for (let i = 0; i < size; i++) {
      const c = vec.get(i);
      arr.push({
        start_x: c.start_x,
        start_y: c.start_y,
        start_z: c.start_z,
        end_x: c.end_x,
        end_y: c.end_y,
        end_z: c.end_z,
        mag: c.mag,
        phase: c.phase,
      });
    }
    vec.delete();
    return arr;
  } catch (e) {
    return [];
  }
};

const extractPatternToJS = (eng) => {
  const vec = eng.calculatePattern(37, 73);
  const arr = [];
  const size = vec.size();
  let maxG = -999;
  for (let i = 0; i < size; i++) {
    const p = vec.get(i);
    arr.push({ theta: p.theta, phi: p.phi, gain: p.total_gain });
    if (p.total_gain > maxG) maxG = p.total_gain;
  }
  vec.delete();
  return { data: arr, maxGain: maxG };
};

const runSingle = () => {
  if (!wasmModule.value || params.wires.length === 0) return;
  status.calculating = true;
  result.hasResult = false;

  setTimeout(() => {
    let eng = null;
    try {
      eng = createEngine();
      eng.setFrequency(Number(params.freq));
      const res = eng.runSimulation();

      if (res.success) {
        result.hasResult = true;
        result.freq = res.freq_mhz;
        result.swr = res.swr_50;
        result.z_r = res.impedance.r;
        result.z_i = res.impedance.i;
        result.currents = extractCurrentsToJS(eng);

        const patData = extractPatternToJS(eng);
        result.pattern = patData.data;
        result.gain = patData.maxGain;

        status.msg = "Calculated";
        nextTick(() => update3DScene());
      } else {
        status.msg = "Error: " + res.error;
      }
    } catch (e) {
      status.msg = "Error: " + e;
    } finally {
      if (eng) eng.delete();
      status.calculating = false;
    }
  }, 10);
};

const runSweep = async (startFreq, endFreq, stepsNum) => {
  if (!wasmModule.value || params.wires.length === 0) return;
  status.calculating = true;
  status.progress = 0;

  sweepData.labels = [];
  sweepData.values = [];
  sweepData.impedance = [];

  const start = Number(startFreq);
  const end = Number(endFreq);
  const steps = parseInt(stepsNum);
  const stepSize = (end - start) / steps;

  for (let i = 0; i <= steps; i++) {
    const f = start + i * stepSize;
    let eng = null;
    try {
      eng = createEngine();
      eng.setFrequency(f);
      const res = eng.runSimulation();
      if (res.success) {
        let val = res.swr_50;
        if (val > 20 || isNaN(val)) val = 20;

        sweepData.labels.push(f);
        sweepData.values.push(val);
        sweepData.impedance.push({ r: res.impedance.r, i: res.impedance.i });
      }
    } catch (e) {
      console.error(e);
    } finally {
      if (eng) eng.delete();
    }

    status.progress = Math.round((i / steps) * 100);
    await new Promise((r) => setTimeout(r, 0));
  }

  status.msg = "Done";
  status.calculating = false;
};

let scene, camera, renderer, controls, animationId;
const objects = [];

const initThree = () => {
  const el = canvasRef.value;
  scene = new THREE.Scene();
  scene.background = new THREE.Color("#ffffff");

  const w = el.clientWidth;
  const h = el.clientHeight;
  camera = new THREE.PerspectiveCamera(45, w / h, 0.01, 10000);
  camera.position.set(2, 2, 2);
  camera.up.set(0, 0, 1);

  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setSize(w, h);
  renderer.setPixelRatio(window.devicePixelRatio);
  el.appendChild(renderer.domElement);

  controls = new OrbitControls(camera, renderer.domElement);

  new ResizeObserver(() => {
    if (!canvasRef.value) return;
    const nw = canvasRef.value.clientWidth;
    const nh = canvasRef.value.clientHeight;
    camera.aspect = nw / nh;
    camera.updateProjectionMatrix();
    renderer.setSize(nw, nh);
  }).observe(el);

  animate();
};

const getSourcePosition = () => {
  if (params.wires.length >= params.source.tag && params.source.tag > 0) {
    const w = params.wires[params.source.tag - 1];
    const x1 = parseFloat(w.x1);
    const y1 = parseFloat(w.y1);
    const z1 = parseFloat(w.z1);
    const x2 = parseFloat(w.x2);
    const y2 = parseFloat(w.y2);
    const z2 = parseFloat(w.z2);
    const segs = parseFloat(w.segs);

    if (![x1, y1, z1, x2, y2, z2, segs].every(Number.isFinite)) {
      return new THREE.Vector3(0, 0, 0);
    }

    const p1 = new THREE.Vector3(x1, y1, z1);
    const p2 = new THREE.Vector3(x2, y2, z2);
    const t = (Number(params.source.seg) - 0.5) / segs;
    return new THREE.Vector3().lerpVectors(p1, p2, t);
  }
  return new THREE.Vector3(0, 0, 0);
};

const fitCameraToObject = () => {
  if (!camera || !controls || params.wires.length === 0) return;
  
  const box = new THREE.Box3();
  params.wires.forEach((w) => {
    if (w.x1 !== undefined && w.x1 !== '') {
      box.expandByPoint(new THREE.Vector3(Number(w.x1), Number(w.y1), Number(w.z1)));
      box.expandByPoint(new THREE.Vector3(Number(w.x2), Number(w.y2), Number(w.z2)));
    }
  });
  
  const size = box.getSize(new THREE.Vector3());
  const maxDim = Math.max(size.x, size.y, size.z) || 1;
  const sourcePos = getSourcePosition();

  controls.target.copy(sourcePos);
  
  const offset = new THREE.Vector3(1, 1, 1)
    .normalize()
    .multiplyScalar(Math.max(2, maxDim * 3));
    
  camera.position.copy(sourcePos).add(offset);
  controls.update();
};

const update3DScene = () => {
  if (!scene) return;

  objects.forEach((o) => {
    scene.remove(o);
    if (o.geometry) o.geometry.dispose();
    if (o.material) o.material.dispose();
  });
  objects.length = 0;

  params.wires.forEach((w, i) => {
    const x1 = parseFloat(w.x1), y1 = parseFloat(w.y1), z1 = parseFloat(w.z1);
    const x2 = parseFloat(w.x2), y2 = parseFloat(w.y2), z2 = parseFloat(w.z2);
    
    if (![x1, y1, z1, x2, y2, z2].every(Number.isFinite)) return;

    const p1 = new THREE.Vector3(x1, y1, z1);
    const p2 = new THREE.Vector3(x2, y2, z2);
    const dist = p1.distanceTo(p2);
    
    if (dist > 0.000001 && Number.isFinite(dist)) {
      const safeRad = Number.isFinite(parseFloat(w.rad)) ? parseFloat(w.rad) : 0.001;
      const geometry = new THREE.CylinderGeometry(safeRad, safeRad, dist, 8, 1);
      const material = new THREE.MeshBasicMaterial({ color: 0x000000 });
      const cyl = new THREE.Mesh(geometry, material);
      
      const midpoint = new THREE.Vector3().lerpVectors(p1, p2, 0.5);
      cyl.position.copy(midpoint);
      
      const axis = new THREE.Vector3(0, 1, 0);
      const tangent = new THREE.Vector3().subVectors(p2, p1).normalize();
      cyl.quaternion.setFromUnitVectors(axis, tangent);
      
      scene.add(cyl);
      objects.push(cyl);
    }

    if (i + 1 === params.source.tag) {
      const sp = getSourcePosition();
      const rawRad = parseFloat(w.rad);
      const safeRad = Number.isFinite(rawRad) ? rawRad : 0.001;

      const sph = new THREE.Mesh(
        new THREE.SphereGeometry(Math.max(0.01, safeRad * 4), 8, 8),
        new THREE.MeshBasicMaterial({ color: 0x000000 })
      );
      sph.position.copy(sp);
      scene.add(sph);
      objects.push(sph);
    }
  });

  if (result.hasResult && ui.showCurrents && result.currents.length > 0) {
    const currents = result.currents;
    const lambda = 299.792458 / (result.freq || 460);

    let maxMag = 0;
    for (let i = 0; i < currents.length; i++)
      if (currents[i].mag > maxMag) maxMag = currents[i].mag;
    if (maxMag === 0) maxMag = 1;

    const box = new THREE.Box3().setFromObject(scene);
    const size = box.getSize(new THREE.Vector3()).length() || 1;
    const visScale = size * ui.scale * 0.5;

    const bboxSize = box.getSize(new THREE.Vector3());
    let prefGlobalNormal = new THREE.Vector3(0, 0, 1);

    if (bboxSize.x < bboxSize.z * 0.1 && bboxSize.x < bboxSize.y * 0.1) {
      prefGlobalNormal.set(1, 0, 0);
    } else if (bboxSize.y < bboxSize.z * 0.1 && bboxSize.y < bboxSize.x * 0.1) {
      prefGlobalNormal.set(0, 1, 0);
    }

    let globalIdx = 0;
    params.wires.forEach((w) => {
      const segs = w.segs;
      if (!Number(w.x1) && w.x1 !== 0) { globalIdx += segs; return; }

      const pts = [];

      const p1 = new THREE.Vector3(Number(w.x1), Number(w.y1), Number(w.z1));
      const p2 = new THREE.Vector3(Number(w.x2), Number(w.y2), Number(w.z2));
      const dir = new THREE.Vector3().subVectors(p2, p1).normalize();

      let normal = prefGlobalNormal
        .clone()
        .sub(dir.clone().multiplyScalar(prefGlobalNormal.dot(dir)));
      if (normal.lengthSq() < 0.01) {
        let alt = new THREE.Vector3(1, 0, 0);
        if (Math.abs(dir.dot(alt)) > 0.9) alt.set(0, 1, 0);
        normal = new THREE.Vector3().crossVectors(dir, alt);
      }
      normal.normalize();

      for (let k = 0; k < segs; k++) {
        const idx = globalIdx + k;
        if (idx >= currents.length) break;
        const c = currents[idx];

        const mid = new THREE.Vector3(
          ((c.start_x + c.end_x) / 2) * lambda,
          ((c.start_y + c.end_y) / 2) * lambda,
          ((c.start_z + c.end_z) / 2) * lambda
        );

        const h = (c.mag / maxMag) * visScale;
        pts.push(mid.clone().addScaledVector(normal, h));
      }

      if (pts.length > 1) {
        const geo = new THREE.BufferGeometry().setFromPoints(pts);
        const mat = new THREE.LineBasicMaterial({ color: 0x000000 });
        const curve = new THREE.Line(geo, mat);
        scene.add(curve);
        objects.push(curve);
      }
      globalIdx += segs;
    });
  }

  if (result.hasResult && ui.showPattern && result.pattern.length > 0) {
    const pat = result.pattern;
    const nTheta = 37,
      nPhi = 73;
    const vertices = [],
      colors = [],
      indices = [];

    const maxG = result.gain;
    const minG = maxG - 40;
    const box = new THREE.Box3().setFromObject(scene);
    const sceneSize = box.getSize(new THREE.Vector3()).length() || 1;
    const rScale = sceneSize * 0.8;

    const sourceCenter = getSourcePosition();

    const getColor = (val) => {
      let norm = (val - minG) / (maxG - minG);
      if (norm < 0) norm = 0;
      if (norm > 1) norm = 1;
      const color = new THREE.Color();
      color.setHSL(0.66 * (1.0 - norm), 1.0, 0.5);
      return color;
    };

    for (let i = 0; i < pat.length; i++) {
      const p = pat[i];
      const thetaRad = (p.theta * Math.PI) / 180;
      const phiRad = (p.phi * Math.PI) / 180;

      let r = p.gain;
      if (r < minG) r = minG;
      let radius = ((r - minG) / (maxG - minG)) * rScale;

      const x = radius * Math.sin(thetaRad) * Math.cos(phiRad);
      const y = radius * Math.sin(thetaRad) * Math.sin(phiRad);
      const z = radius * Math.cos(thetaRad);

      vertices.push(x + sourceCenter.x, y + sourceCenter.y, z + sourceCenter.z);
      const c = getColor(p.gain);
      colors.push(c.r, c.g, c.b);
    }

    for (let t = 0; t < nTheta - 1; t++) {
      for (let p = 0; p < nPhi - 1; p++) {
        const a = t * nPhi + p;
        const b = (t + 1) * nPhi + p;
        const c = t * nPhi + (p + 1);
        const d = (t + 1) * nPhi + (p + 1);
        indices.push(a, b, d);
        indices.push(a, d, c);
      }
    }

    const geo = new THREE.BufferGeometry();
    geo.setAttribute("position", new THREE.Float32BufferAttribute(vertices, 3));
    geo.setAttribute("color", new THREE.Float32BufferAttribute(colors, 3));
    geo.setIndex(indices);
    geo.computeVertexNormals();

    const mat = new THREE.MeshBasicMaterial({
      vertexColors: true,
      side: THREE.DoubleSide,
      transparent: true,
      opacity: 0.6,
      wireframe: true,
    });
    const mesh = new THREE.Mesh(geo, mat);
    scene.add(mesh);
    objects.push(mesh);
  }
};

watch(() => [params.wires, ui.scale, ui.showCurrents, ui.showPattern], update3DScene, {
  deep: true,
});

const animate = () => {
  animationId = requestAnimationFrame(animate);
  if (controls) controls.update();
  if (renderer && scene && camera) renderer.render(scene, camera);
};

const addWire = () =>
  params.wires.push({
    x1: undefined,
    y1: undefined,
    z1: undefined,
    x2: undefined,
    y2: undefined,
    z2: undefined,
    rad: undefined,
    segs: undefined,
  });
const delWire = (i) => params.wires.splice(i, 1);
</script>

<template>
  <div class="layout">
    <div class="viewport" ref="canvasRef"></div>
    <button class="toggle" @click="ui.sidebarOpen = !ui.sidebarOpen">
      {{ ui.sidebarOpen ? "<<" : ">>" }}
    </button>
    <div class="sidebar" :class="{ closed: !ui.sidebarOpen }">
      <h3>nec2web</h3>
      <select @change="loadPreset(presets[$event.target.value])">
        <option disabled selected>Load Preset...</option>
        <option v-for="(p, i) in presets" :key="i" :value="i">
          {{ p.name }}
        </option>
      </select>
      <hr />
      <div>
        <input type="radio" id="mode1" value="single" v-model="ui.tab" />
        <label for="mode1">Single</label>
        <input type="radio" id="mode2" value="sweep" v-model="ui.tab" />
        <label for="mode2">Analysis</label>
      </div>
      <hr />
      <div v-if="ui.tab === 'single'" class="scroll-content">
        <details open>
          <summary>View</summary>
          <div>
            Scale
            <input
              type="range"
              min="0.01"
              max="0.5"
              step="0.01"
              v-model.number="ui.scale"
            />
          </div>
          <div>
            <input type="checkbox" id="curr" v-model="ui.showCurrents" /><label for="curr"
              >Currents</label
            >
          </div>
          <div>
            <input type="checkbox" id="patt" v-model="ui.showPattern" /><label for="patt"
              >3D Pattern</label
            >
          </div>
        </details>
        <div style="margin-top: 10px">
          Freq (MHz) <input type="number" v-model.number="params.freq" />
        </div>
        <details open style="margin-top: 10px">
          <summary>Wires ({{ params.wires.length }})</summary>
          <button @click="addWire">Add</button>
          <div v-for="(w, i) in params.wires" :key="i" class="wire-item">
            <div style="display: flex">
              <b>#{{ i + 1 }}</b>
              <a href="javascript:void(0)" @click="delWire(i)" style="margin-left: auto"
                >Del</a
              >
            </div>

            <div class="coords-stack">
              <input v-model.number="w.x1" class="full" placeholder="x1" />
              <input v-model.number="w.y1" class="full" placeholder="y1" />
              <input v-model.number="w.z1" class="full" placeholder="z1" />
              <div class="to-link">to</div>
              <input v-model.number="w.x2" class="full" placeholder="x2" />
              <input v-model.number="w.y2" class="full" placeholder="y2" />
              <input v-model.number="w.z2" class="full" placeholder="z2" />
            </div>

            <div class="param-row">
              <label>R</label>
              <input v-model.number="w.rad" placeholder="m" />
            </div>
            <div class="param-row">
              <label>Seg</label>
              <input v-model.number="w.segs" placeholder="Count" />
            </div>
          </div>
        </details>
        <fieldset style="margin-top: 10px">
          <legend>Source</legend>
          Tag <input v-model.number="params.source.tag" class="short" /> Seg
          <input v-model.number="params.source.seg" class="short" />
        </fieldset>
        <button
          @click="runSingle"
          :disabled="status.calculating"
          style="margin-top: 10px"
        >
          Calculate
        </button>
        <table v-if="result.hasResult" style="width: 100%; border-collapse: collapse; margin-top: 10px;">
          <tbody>
            <tr>
              <td>SWR</td>
              <td style="text-align: right;">{{ result.swr.toFixed(3) }}</td>
            </tr>
            <tr>
              <td>Max Gain</td>
              <td style="text-align: right;">{{ result.gain.toFixed(2) }} dBi</td>
            </tr>
            <tr>
              <td>Z</td>
              <td style="text-align: right;">{{ result.z_r.toFixed(1) }} {{ result.z_i >= 0 ? "+" : "" }}{{ result.z_i.toFixed(1) }}j</td>
            </tr>
          </tbody>
        </table>
      </div>
      <div v-else class="analysis-panel">
        <AnalysisCharts
          :data="sweepData"
          v-model="sweepParams"
          :on-run="runSweep"
          :is-calculating="status.calculating"
        />
      </div>
      <div style="margin-top: auto; padding-top: 10px; text-align: center">
        <a href="https://github.com/undef-i/nec2web" target="_blank">GitHub</a>
      </div>
    </div>
  </div>
</template>

<style scoped>
.layout {
  position: relative;
  width: 100vw;
  height: 100vh;
  overflow: hidden;
  font-family: sans-serif;
}
.viewport {
  width: 100%;
  height: 100%;
}
.toggle {
  position: absolute;
  top: 10px;
  left: 10px;
  z-index: 20;
}
.sidebar {
  position: absolute;
  top: 0;
  left: 0;
  bottom: 0;
  width: 340px;
  max-width: 100vw;
  background: white;
  border-right: 1px solid black;
  z-index: 10;
  padding: 10px;
  transform: translateX(0);
  transition: transform 0.1s;
  display: flex;
  flex-direction: column;
}
.sidebar.closed {
  transform: translateX(-100%);
}
.scroll-content {
  overflow-y: auto;
  flex: 1;
}
.analysis-panel {
  display: flex;
  flex-direction: column;
  height: 100%;
  overflow-y: auto;
}
summary {
  cursor: pointer;
  user-select: none;
}
.wire-item {
  border-top: 1px solid #ccc;
  margin-top: 5px;
  padding-top: 2px;
}
.coords-stack {
  display: flex;
  flex-direction: column;
}

.coord {
  width: 50px;
}
.short {
  width: 35px;
}

.param-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
}
</style>