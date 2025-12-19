<script setup>
import {
  ref,
  reactive,
  onMounted,
  onBeforeUnmount,
  watch,
  shallowRef,
  nextTick,
} from "vue";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import Chart from "chart.js/auto";

const presets = [
  {
    name: "CJU (460MHz)",
    freq: 460.0,
    source: { tag: 4, seg: 6, real: 1.0, imag: 0.0 },
    wires: [
      {
        x1: -0.1115,
        y1: 0.0,
        z1: 0.0,
        x2: -0.1115,
        y2: -0.107,
        z2: 0.0,
        rad: 0.0022,
        segs: 5,
      },
      {
        x1: -0.1115,
        y1: -0.107,
        z1: 0.0,
        x2: 0.1115,
        y2: -0.107,
        z2: 0.0,
        rad: 0.0022,
        segs: 11,
      },
      {
        x1: 0.1115,
        y1: -0.107,
        z1: 0.0,
        x2: 0.1115,
        y2: 0.0,
        z2: 0.0,
        rad: 0.0022,
        segs: 5,
      },
      {
        x1: 0.1115,
        y1: 0.0,
        z1: 0.0,
        x2: -0.1115,
        y2: 0.0,
        z2: 0.0,
        rad: 0.0022,
        segs: 11,
      },
      {
        x1: -0.1615,
        y1: 0.042,
        z1: 0.0,
        x2: 0.1615,
        y2: 0.042,
        z2: 0.0,
        rad: 0.0022,
        segs: 15,
      },
    ],
  },
  {
    name: "Horn 6-el Yagi (145MHz)",
    freq: 145.0,
    source: { tag: 10, seg: 8, real: 1.0, imag: 0.0 },
    wires: [
      {
        x1: -0.4,
        y1: 0.562,
        z1: 0.0,
        x2: -0.4,
        y2: -0.562,
        z2: 0.0,
        rad: 0.002,
        segs: 10,
      },
      {
        x1: 0.3,
        y1: 0.4675,
        z1: -0.1,
        x2: 0.3,
        y2: -0.4675,
        z2: -0.1,
        rad: 0.002,
        segs: 10,
      },
      {
        x1: 0.3,
        y1: 0.4675,
        z1: 0.1,
        x2: 0.3,
        y2: -0.4675,
        z2: 0.1,
        rad: 0.002,
        segs: 10,
      },
      {
        x1: 0.6,
        y1: 0.465,
        z1: 0.25,
        x2: 0.6,
        y2: -0.465,
        z2: 0.25,
        rad: 0.002,
        segs: 10,
      },
      {
        x1: 0.6,
        y1: 0.465,
        z1: -0.25,
        x2: 0.6,
        y2: -0.465,
        z2: -0.25,
        rad: 0.002,
        segs: 10,
      },
      {
        x1: 0.95,
        y1: 0.4625,
        z1: -0.4,
        x2: 0.95,
        y2: -0.4625,
        z2: -0.4,
        rad: 0.002,
        segs: 10,
      },
      {
        x1: 0.95,
        y1: 0.4625,
        z1: 0.4,
        x2: 0.95,
        y2: -0.4625,
        z2: 0.4,
        rad: 0.002,
        segs: 10,
      },
      {
        x1: 1.4,
        y1: 0.46,
        z1: 0.55,
        x2: 1.4,
        y2: -0.46,
        z2: 0.55,
        rad: 0.002,
        segs: 10,
      },
      {
        x1: 1.4,
        y1: 0.46,
        z1: -0.55,
        x2: 1.4,
        y2: -0.46,
        z2: -0.55,
        rad: 0.002,
        segs: 10,
      },
      {
        x1: 0.0,
        y1: 0.494,
        z1: 0.0,
        x2: 0.0,
        y2: -0.494,
        z2: 0.0,
        rad: 0.002,
        segs: 10,
      },
      {
        x1: -0.4,
        y1: 0.562,
        z1: 0.4,
        x2: -0.4,
        y2: -0.562,
        z2: 0.4,
        rad: 0.002,
        segs: 10,
      },
      {
        x1: -0.4,
        y1: 0.562,
        z1: -0.4,
        x2: -0.4,
        y2: -0.562,
        z2: -0.4,
        rad: 0.002,
        segs: 10,
      },
    ],
  },
  {
    name: "80m Dipole",
    freq: 3.6,
    source: { tag: 1, seg: 10, real: 1.0, imag: 0.0 },
    wires: [
      {
        x1: -19.64,
        y1: 0.0,
        z1: 20.0,
        x2: 19.64,
        y2: 0.0,
        z2: 20.0,
        rad: 0.001,
        segs: 19,
      },
    ],
  },
];

const wasmModule = ref(null);
const canvasRef = ref(null);
const swrChartRef = ref(null);
const smithChartRef = ref(null);

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
  if (chartInstance.value) chartInstance.value.destroy();
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

const runSweep = async () => {
  if (!wasmModule.value || params.wires.length === 0) return;
  status.calculating = true;
  status.progress = 0;

  sweepData.labels = [];
  sweepData.values = [];
  sweepData.impedance = [];

  if (!chartInstance.value) initSWRChart();

  if (chartInstance.value) {
    chartInstance.value.data.labels = [];
    chartInstance.value.data.datasets[0].data = [];
    chartInstance.value.update("none");
  }
  drawSmithChart(true);

  const start = Number(sweepParams.start);
  const end = Number(sweepParams.end);
  const steps = parseInt(sweepParams.steps);
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
        sweepData.labels.push(f.toFixed(1));
        sweepData.values.push(val);
        sweepData.impedance.push({ r: res.impedance.r, i: res.impedance.i });
      }
    } catch (e) {
    } finally {
      if (eng) eng.delete();
    }

    status.progress = Math.round((i / steps) * 100);
    await new Promise((r) => setTimeout(r, 0));
  }

  updateChart();
  drawSmithChart(false);
  status.msg = "Done";
  status.calculating = false;
};

const chartInstance = shallowRef(null);

const initSWRChart = () => {
  if (!swrChartRef.value) return;
  if (chartInstance.value) chartInstance.value.destroy();

  const ctx = swrChartRef.value.getContext("2d");
  chartInstance.value = new Chart(ctx, {
    type: "line",
    data: {
      labels: [],
      datasets: [
        {
          label: "SWR",
          data: [],
          borderColor: "#000000",
          borderWidth: 1,
          pointRadius: 0,
          tension: 0,
          fill: false,
        },
      ],
    },
    options: {
      animation: false,
      responsive: true,
      maintainAspectRatio: false,
      plugins: { legend: { display: false } },
      scales: {
        x: { grid: { display: false } },
        y: { min: 1.0, title: { display: true, text: "SWR" } },
      },
    },
  });
};

const updateChart = () => {
  if (!chartInstance.value) initSWRChart();
  if (chartInstance.value) {
    chartInstance.value.data.labels = sweepData.labels;
    chartInstance.value.data.datasets[0].data = sweepData.values;
    chartInstance.value.update();
  }
};

const drawSmithChart = (clearOnly = false) => {
  if (!smithChartRef.value) return;
  const canvas = smithChartRef.value;
  const ctx = canvas.getContext("2d");
  const w = canvas.width;
  const h = canvas.height;
  const cx = w / 2;
  const cy = h / 2;
  const r = Math.min(w, h) / 2 - 5;

  ctx.fillStyle = "#fff";
  ctx.fillRect(0, 0, w, h);

  if (clearOnly) return;

  const g2p = (u, v) => ({ x: cx + u * r, y: cy - v * r });

  ctx.lineWidth = 0.5;
  ctx.strokeStyle = "#ccc";
  const drawRCircle = (res) => {
    const g_center_u = res / (1 + res);
    const g_radius = 1 / (1 + res);
    ctx.beginPath();
    ctx.arc(cx + g_center_u * r, cy, g_radius * r, 0, 2 * Math.PI);
    ctx.stroke();
  };
  [0, 0.5, 1.0, 2.0, 5.0].forEach(drawRCircle);

  ctx.strokeStyle = "#000";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, 2 * Math.PI);
  ctx.stroke();
  ctx.beginPath();
  ctx.moveTo(0, cy);
  ctx.lineTo(w, cy);
  ctx.stroke();

  if (sweepData.impedance.length === 0) return;

  ctx.lineWidth = 1.5;
  ctx.strokeStyle = "#0000FF";
  ctx.beginPath();

  let started = false;
  sweepData.impedance.forEach((z) => {
    const z0 = 50;
    const rn = z.r / z0;
    const xn = z.i / z0;
    const d = (rn + 1) ** 2 + xn ** 2;
    if (d === 0) return;
    const u = (rn ** 2 + xn ** 2 - 1) / d;
    const v = (2 * xn) / d;

    const pos = g2p(u, v);
    if (!started) {
      ctx.moveTo(pos.x, pos.y);
      started = true;
    } else {
      ctx.lineTo(pos.x, pos.y);
    }
  });
  ctx.stroke();
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

const fitCameraToObject = () => {
  if (!camera || !controls) return;
  const box = new THREE.Box3();
  params.wires.forEach((w) => {
    box.expandByPoint(new THREE.Vector3(w.x1, w.y1, w.z1));
    box.expandByPoint(new THREE.Vector3(w.x2, w.y2, w.z2));
  });
  if (box.isEmpty()) return;
  const center = box.getCenter(new THREE.Vector3());
  const size = box.getSize(new THREE.Vector3());
  const maxDim = Math.max(size.x, size.y, size.z);
  controls.target.copy(center);
  const offset = new THREE.Vector3(1, 1, 1)
    .normalize()
    .multiplyScalar(Math.max(2, maxDim * 3));
  camera.position.copy(center).add(offset);
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

  const wireMat = new THREE.LineBasicMaterial({ color: 0x000000 });
  params.wires.forEach((w, i) => {
    const p1 = new THREE.Vector3(w.x1, w.y1, w.z1);
    const p2 = new THREE.Vector3(w.x2, w.y2, w.z2);
    const line = new THREE.Line(
      new THREE.BufferGeometry().setFromPoints([p1, p2]),
      wireMat
    );
    scene.add(line);
    objects.push(line);

    if (i + 1 === params.source.tag) {
      const t = (params.source.seg - 0.5) / w.segs;
      const sp = new THREE.Vector3().lerpVectors(p1, p2, t);
      const sph = new THREE.Mesh(
        new THREE.SphereGeometry(Math.max(0.01, w.rad * 4), 8, 8),
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
      const pts = [];

      const p1 = new THREE.Vector3(w.x1, w.y1, w.z1);
      const p2 = new THREE.Vector3(w.x2, w.y2, w.z2);
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

      vertices.push(x, y, z);
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

watch(
  () => [params.wires, ui.scale, ui.showCurrents, ui.showPattern],
  update3DScene,
  { deep: true }
);

const animate = () => {
  animationId = requestAnimationFrame(animate);
  if (controls) controls.update();
  if (renderer && scene && camera) renderer.render(scene, camera);
};

const addWire = () =>
  params.wires.push({
    x1: 0,
    y1: 0,
    z1: 0,
    x2: 0.1,
    y2: 0,
    z2: 0,
    rad: 0.001,
    segs: 5,
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
        &nbsp;
        <input type="radio" id="mode2" value="sweep" v-model="ui.tab" />
        <label for="mode2">Analysis</label>
      </div>
      <hr />

      <div v-if="ui.tab === 'single'">
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
            <input type="checkbox" id="curr" v-model="ui.showCurrents" /><label
              for="curr"
              >Currents</label
            >
          </div>
          <div>
            <input type="checkbox" id="patt" v-model="ui.showPattern" /><label
              for="patt"
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
              <a
                href="javascript:void(0)"
                @click="delWire(i)"
                style="margin-left: auto"
                >Del</a
              >
            </div>
            <div>
              <input v-model.number="w.x1" class="coord" /><input
                v-model.number="w.y1"
                class="coord"
              /><input v-model.number="w.z1" class="coord" />
              to
              <input v-model.number="w.x2" class="coord" /><input
                v-model.number="w.y2"
                class="coord"
              /><input v-model.number="w.z2" class="coord" />
            </div>
            <div>
              R <input v-model.number="w.rad" class="short" /> Seg
              <input v-model.number="w.segs" class="short" />
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

        <div
          v-if="result.hasResult"
          style="margin-top: 10px; border: 1px solid black; padding: 5px"
        >
          <div>SWR: {{ result.swr.toFixed(3) }}</div>
          <div>Max Gain: {{ result.gain.toFixed(2) }} dBi</div>
          <div>
            Z: {{ result.z_r.toFixed(1) }} {{ result.z_i >= 0 ? "+" : ""
            }}{{ result.z_i.toFixed(1) }}j
          </div>
        </div>
      </div>

      <div v-else>
        <fieldset>
          <legend>Range</legend>
          Start <input type="number" v-model.number="sweepParams.start" /><br />
          End <input type="number" v-model.number="sweepParams.end" /><br />
          Steps <input type="number" v-model.number="sweepParams.steps" />
        </fieldset>

        <button
          @click="runSweep"
          :disabled="status.calculating"
          style="margin-top: 10px"
        >
          Run Analysis
        </button>

        <div class="chart-box">
          <div
            style="
              font-size: 10px;
              text-align: center;
              border-bottom: 1px solid #ccc;
            "
          >
            SWR
          </div>
          <div style="flex: 1; position: relative">
            <canvas ref="swrChartRef"></canvas>
          </div>
        </div>

        <div class="chart-box" style="height: 250px">
          <div
            style="
              font-size: 10px;
              text-align: center;
              border-bottom: 1px solid #ccc;
            "
          >
            Smith Chart
          </div>
          <div style="flex: 1; position: relative">
            <canvas ref="smithChartRef" width="300" height="230"></canvas>
          </div>
        </div>
      </div>

      <div
        style="
          margin-top: auto;
          padding-top: 10px;
          border-top: 1px solid #ccc;
          text-align: center;
        "
      >
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
  max-width: 320px;
  background: white;
  border-right: 1px solid black;
  z-index: 10;
  padding: 10px;
  overflow-y: auto;
  transform: translateX(0);
  transition: transform 0.1s;
  display: flex;
  flex-direction: column;
}
.sidebar.closed {
  transform: translateX(-100%);
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
.coord {
  width: 40px;
}
.short {
  width: 35px;
}
.chart-box {
  height: 150px;
  border: 1px solid black;
  margin-top: 10px;
  display: flex;
  flex-direction: column;
}
canvas {
  width: 100%;
  height: 100%;
}
a {
  color: blue;
  text-decoration: underline;
  cursor: pointer;
}
</style>