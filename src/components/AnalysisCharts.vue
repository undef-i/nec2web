<script setup>
import { ref, computed, onMounted, onUnmounted, watch } from "vue";

const props = defineProps({
  data: {
    type: Object,
    required: true,
  },
  modelValue: {
    type: Object,
    required: true, 
  },
  onRun: {
    type: Function,
    required: true,
  },
  isCalculating: {
    type: Boolean,
    default: false,
  }
});

const emit = defineEmits(["update:modelValue"]);

const container = ref(null);
const width = ref(300);
const swrHeight = 150;
const smithSize = 300;
const hoverIndex = ref(-1);
const optimizeIter = ref(3);

const localRange = ref({ ...props.modelValue });

watch(() => props.modelValue, (newVal) => {
  localRange.value = { ...newVal };
}, { deep: true });

const syncRange = () => {
  emit("update:modelValue", { ...localRange.value });
};

let resizeObserver;
onMounted(() => {
  if (container.value) {
    resizeObserver = new ResizeObserver((entries) => {
      width.value = entries[0].contentRect.width;
    });
    resizeObserver.observe(container.value);
  }
});
onUnmounted(() => {
  if (resizeObserver) resizeObserver.disconnect();
});

const getSmoothPath = (points) => {
  if (points.length < 2) return "";
  let d = `M ${points[0].x} ${points[0].y}`;
  for (let i = 0; i < points.length - 1; i++) {
    const p0 = points[i === 0 ? 0 : i - 1];
    const p1 = points[i];
    const p2 = points[i + 1];
    const p3 = points[i + 2] || p2;
    const cp1x = p1.x + (p2.x - p0.x) / 6;
    const cp1y = p1.y + (p2.y - p0.y) / 6;
    const cp2x = p2.x - (p3.x - p1.x) / 6;
    const cp2y = p2.y - (p3.y - p1.y) / 6;
    d += ` C ${cp1x} ${cp1y}, ${cp2x} ${cp2y}, ${p2.x} ${p2.y}`;
  }
  return d;
};

const minFreq = computed(() => Number(localRange.value.start));
const maxFreq = computed(() => Number(localRange.value.end));
const freqRange = computed(() => maxFreq.value - minFreq.value || 1);

const formatFreq = (val) => {
  const range = freqRange.value;
  const v = Number(val);
  if (range <= 0.1) return v.toFixed(5);
  if (range <= 1.0) return v.toFixed(4);
  if (range <= 10.0) return v.toFixed(3);
  if (range <= 100.0) return v.toFixed(2);
  return v.toFixed(1);
};

const maxDataSwr = computed(() => {
  if (!props.data.values || !props.data.values.length) return 5;
  return Math.max(3, ...props.data.values);
});

const mapFreqX = (f) => {
  const pct = (f - minFreq.value) / freqRange.value;
  return pct * width.value;
};

const mapSwrY = (s) => {
  const minS = 1.0;
  const maxS = maxDataSwr.value;
  const val = Math.max(minS, s);
  const pct = (val - minS) / (maxS - minS || 1);
  return swrHeight - pct * swrHeight;
};

const smithR = smithSize / 2 - 2;
const smithCx = smithSize / 2;
const smithCy = smithSize / 2;

const getSmithPt = (r, x) => {
  const z0 = 50;
  const rn = r / z0;
  const xn = x / z0;
  const den = (rn + 1) ** 2 + xn ** 2;
  if (den === 0) return { x: smithCx - smithR, y: smithCy }; 

  const u = (rn ** 2 + xn ** 2 - 1) / den;
  const v = (2 * xn) / den;
  return {
    x: smithCx + u * smithR,
    y: smithCy - v * smithR,
  };
};

const smithGrids = computed(() => {
  const rValues = [0, 0.2, 0.5, 1.0, 2.0, 5.0];
  const xValues = [0.2, 0.5, 1.0, 2.0, 5.0];

  const rCircles = rValues.map((rVal) => {
    const u = rVal / (1 + rVal);
    const rad = 1 / (1 + rVal);
    return {
      cx: smithCx + u * smithR,
      cy: smithCy,
      r: rad * smithR,
      val: rVal,
      isPrime: rVal === 1.0 || rVal === 0,
    };
  });

  const xCircles = [];
  xValues.forEach((xVal) => {
    const rad = (1 / xVal) * smithR;
    const cx = smithCx + smithR; 
    xCircles.push({
      cx: cx,
      cy: smithCy - (1 / xVal) * smithR,
      r: rad,
      isPrime: xVal === 1.0,
    });
    xCircles.push({
      cx: cx,
      cy: smithCy + (1 / xVal) * smithR,
      r: rad,
      isPrime: xVal === 1.0,
    });
  });

  return { rCircles, xCircles };
});

const swrPoints = computed(() => {
  if (!props.data.labels || !props.data.values) return [];
  return props.data.labels.map((l, i) => ({
    x: mapFreqX(Number(l)),
    y: mapSwrY(props.data.values[i]),
  }));
});
const swrPath = computed(() => getSmoothPath(swrPoints.value));

const smithPoints = computed(() => {
  if (!props.data.impedance) return [];
  return props.data.impedance.map((z) => getSmithPt(z.r, z.i));
});
const smithPath = computed(() => getSmoothPath(smithPoints.value));

const updateHoverFromX = (x) => {
  const freq = minFreq.value + (x / width.value) * freqRange.value;
  let closestIdx = -1;
  let minDiff = Infinity;
  if (props.data.labels) {
    props.data.labels.forEach((l, i) => {
      const f = Number(l);
      const diff = Math.abs(f - freq);
      if (diff < minDiff) {
        minDiff = diff;
        closestIdx = i;
      }
    });
  }
  hoverIndex.value = closestIdx;
};

const onSwrMove = (e) => {
  const rect = e.currentTarget.getBoundingClientRect();
  updateHoverFromX(e.clientX - rect.left);
};

const onSwrTouch = (e) => {
  const rect = e.currentTarget.getBoundingClientRect();
  if (e.touches && e.touches[0]) {
    updateHoverFromX(e.touches[0].clientX - rect.left);
  }
};

const updateHoverFromSmith = (mx, my, rect) => {
  const scaleX = smithSize / rect.width;
  const scaleY = smithSize / rect.height;
  const mouseX = mx * scaleX;
  const mouseY = my * scaleY;
  let closestIdx = -1;
  let minDistSq = Infinity;
  smithPoints.value.forEach((pt, i) => {
    const dx = pt.x - mouseX;
    const dy = pt.y - mouseY;
    const distSq = dx * dx + dy * dy;
    if (distSq < minDistSq) {
      minDistSq = distSq;
      closestIdx = i;
    }
  });
  hoverIndex.value = closestIdx;
};

const onSmithMove = (e) => {
  const rect = e.currentTarget.getBoundingClientRect();
  updateHoverFromSmith(e.clientX - rect.left, e.clientY - rect.top, rect);
};

const onSmithTouch = (e) => {
  const rect = e.currentTarget.getBoundingClientRect();
  if (e.touches && e.touches[0]) {
    updateHoverFromSmith(
      e.touches[0].clientX - rect.left,
      e.touches[0].clientY - rect.top,
      rect
    );
  }
};

const onLeave = () => {
  hoverIndex.value = -1;
};

const activeInfo = computed(() => {
  if (
    hoverIndex.value === -1 ||
    !props.data.labels ||
    !props.data.labels[hoverIndex.value]
  )
    return null;

  const i = hoverIndex.value;
  const rawFreq = Number(props.data.labels[i]);
  const swr = props.data.values[i];
  const z = props.data.impedance[i];

  return {
    freq: formatFreq(rawFreq),
    swr: swr.toFixed(3),
    zReal: z.r.toFixed(2),
    zImag: z.i.toFixed(2),
    swrPt: { x: mapFreqX(rawFreq), y: mapSwrY(swr) },
    smithPt: getSmithPt(z.r, z.i),
  };
});

const xTicks = computed(() => {
  const steps = 5;
  const arr = [];
  for (let i = 0; i <= steps; i++) {
    const f = minFreq.value + (i / steps) * freqRange.value;
    arr.push({ x: (i / steps) * width.value, val: formatFreq(f) });
  }
  return arr;
});

const yTicks = computed(() => {
  const steps = 4;
  const max = maxDataSwr.value;
  const min = 1.0;
  const arr = [];
  for (let i = 0; i <= steps; i++) {
    const val = min + (i / steps) * (max - min);
    arr.push({ y: mapSwrY(val), val: val.toFixed(2) });
  }
  return arr;
});

const runAnalysis = async () => {
  syncRange();
  await props.onRun(
    Number(localRange.value.start),
    Number(localRange.value.end),
    Number(localRange.value.steps)
  );
};

const runOptimization = async () => {
  if (props.isCalculating) return;
  
  if (!props.data.values || props.data.values.length === 0) {
    await runAnalysis();
  }

  const maxIter = parseInt(optimizeIter.value) || 3;
  let currentIter = 0;

  while (currentIter < maxIter) {
    const values = props.data.values;
    const labels = props.data.labels;

    if (!values || values.length === 0) break;

    let minIdx = 0;
    let minSwr = 999;
    for (let i = 0; i < values.length; i++) {
      if (values[i] < minSwr) {
        minSwr = values[i];
        minIdx = i;
      }
    }

    if (minIdx === 0 || minIdx === labels.length - 1) break;

    const newStart = Number(labels[Math.max(0, minIdx - 2)]);
    const newEnd = Number(labels[Math.min(labels.length - 1, minIdx + 2)]);

    if (Math.abs(newEnd - newStart) < 0.00001) break;

    localRange.value.start = newStart;
    localRange.value.end = newEnd;
    syncRange();

    await props.onRun(newStart, newEnd, Number(localRange.value.steps));
    
    currentIter++;
  }
};
</script>

<template>
  <div class="charts-root" ref="container">
    
    <div class="control-panel">
      <div class="row">
        <label>Start</label>
        <input type="number" v-model.number="localRange.start" @change="syncRange" />
      </div>
      <div class="row">
        <label>End</label>
        <input type="number" v-model.number="localRange.end" @change="syncRange" />
      </div>
      <div class="row">
        <label>Steps</label>
        <input type="number" v-model.number="localRange.steps" @change="syncRange" />
      </div>
      <button @click="runAnalysis" :disabled="isCalculating" class="run-btn">
        Run Analysis
      </button>
    </div>

    <div class="opt-panel">
      <div class="row">
        <label>Iter</label>
        <input type="number" v-model.number="optimizeIter" min="1" max="10" />
      </div>
      <button @click="runOptimization" :disabled="isCalculating" class="opt-btn">
        Find Min SWR
      </button>
    </div>

    <table class="data-table">
      <tbody>
        <tr>
          <td>Freq</td>
          <td>{{ activeInfo ? activeInfo.freq + ' MHz' : '--' }}</td>
        </tr>
        <tr>
          <td>SWR</td>
          <td>{{ activeInfo ? activeInfo.swr : '--' }}</td>
        </tr>
        <tr>
          <td>Z</td>
          <td>
            {{ activeInfo ? (activeInfo.zReal + (activeInfo.zImag >= 0 ? '+' : '') + activeInfo.zImag + 'j') : '--' }}
          </td>
        </tr>
      </tbody>
    </table>

    <div class="chart-wrapper">
      <div class="axis-label-y">SWR</div>
      <svg
        class="chart-svg"
        :viewBox="`0 0 ${width} ${swrHeight}`"
        preserveAspectRatio="none"
        @mousemove="onSwrMove"
        @touchmove.prevent="onSwrTouch"
        @touchstart.prevent="onSwrTouch"
        @mouseleave="onLeave"
      >
        <g class="grid">
          <line
            v-for="t in yTicks"
            :key="'ygrid-'+t.val"
            x1="0"
            :y1="t.y"
            :x2="width"
            :y2="t.y"
          />
          <line
            v-for="t in xTicks"
            :key="'xgrid-'+t.val"
            :x1="t.x"
            y1="0"
            :x2="t.x"
            :y2="swrHeight"
          />
        </g>
        <path :d="swrPath" fill="none" stroke="#000" stroke-width="1.5" />
        <circle
          v-if="activeInfo"
          :cx="activeInfo.swrPt.x"
          :cy="activeInfo.swrPt.y"
          r="4"
          fill="#000"
        />
        <g class="axis-text">
          <text
            v-for="(t, i) in yTicks"
            :key="'ylabel-'+t.val"
            v-show="i !== 0"
            x="2"
            :y="t.y - 3"
          >
            {{ t.val }}
          </text>
          <text
            v-for="(t, i) in xTicks"
            :key="'xlabel-'+t.val"
            :x="t.x"
            :y="swrHeight - 3"
            :text-anchor="
              i === 0 ? 'start' : i === xTicks.length - 1 ? 'end' : 'middle'
            "
          >
            {{ t.val }}
          </text>
        </g>
      </svg>
    </div>

    <div class="chart-wrapper smith-wrapper">
      <div class="axis-label-y" style="top: 2px; right: 5px; left: auto;">Smith</div>
      <svg
        class="smith-svg"
        :viewBox="`0 0 ${smithSize} ${smithSize}`"
        @mousemove="onSmithMove"
        @touchmove.prevent="onSmithTouch"
        @touchstart.prevent="onSmithTouch"
        @mouseleave="onLeave"
      >
        <defs>
          <clipPath id="smith-clip">
            <circle :cx="smithCx" :cy="smithCy" :r="smithR" />
          </clipPath>
        </defs>

        <g class="smith-grid">
          <circle
            :cx="smithCx"
            :cy="smithCy"
            :r="smithR"
            fill="#fff"
            stroke="none"
          />
          <g clip-path="url(#smith-clip)">
             <circle 
                v-for="(c, i) in smithGrids.xCircles"
                :key="'x-'+i"
                :cx="c.cx"
                :cy="c.cy"
                :r="c.r"
                fill="none"
                :stroke="c.isPrime ? '#ccc' : '#eee'"
             />
          </g>
          <circle
            v-for="(c, i) in smithGrids.rCircles"
            :key="'r-'+i"
            :cx="c.cx"
            :cy="c.cy"
            :r="c.r"
            fill="none"
            :stroke="c.isPrime ? '#ccc' : '#eee'"
          />
          <line
            :x1="smithCx - smithR"
            :y1="smithCy"
            :x2="smithCx + smithR"
            :y2="smithCy"
            stroke="#ccc"
          />
          <circle
            :cx="smithCx"
            :cy="smithCy"
            :r="smithR"
            fill="none"
            stroke="#999"
          />
        </g>

        <path :d="smithPath" fill="none" stroke="#000" stroke-width="2" />
        
        <circle
          v-if="activeInfo"
          :cx="activeInfo.smithPt.x"
          :cy="activeInfo.smithPt.y"
          r="4"
          fill="#000"
          stroke="#fff"
          stroke-width="1"
        />
        
        <text :x="smithCx - smithR + 2" :y="smithCy - 2" class="smith-label">0Ω</text>
        <text :x="smithCx + smithR - 15" :y="smithCy - 2" class="smith-label">∞</text>
        <text :x="smithCx - 2" :y="smithCy - 2" class="smith-label" text-anchor="end">50Ω</text>
      </svg>
    </div>
  </div>
</template>

<style scoped>
.charts-root {
  display: flex;
  flex-direction: column;
  gap: 10px;
  width: 100%;
}


.opt-panel {
  flex-direction: row;
  align-items: center;
  justify-content: space-between;
}

.row {
  display: flex;
  justify-content: space-between;
  align-items: center;
}












.data-table td:last-child {
  text-align: right;
}

.chart-wrapper {
  position: relative;
  border: 1px solid #ccc;
  background: #fff;
  padding: 0;
  border-radius: 2px;
}

.chart-svg {
  width: 100%;
  height: 150px;
  display: block;
  overflow: hidden;
  cursor: crosshair;
}

.smith-wrapper {
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 10px;
  background: #fafafa;
}

.smith-svg {
  width: 100%;
  max-width: 300px;
  aspect-ratio: 1/1;
  display: block;
  cursor: crosshair;
}

.grid line {
  stroke: #eee;
  stroke-width: 1;
}

.smith-grid circle,
.smith-grid line {
  stroke-width: 1;
  vector-effect: non-scaling-stroke;
}

.smith-label {
  font-size: 10px;
  fill: #888;
  font-family: sans-serif;
  pointer-events: none;
}

.axis-text text {
  font-size: 10px;
  fill: #555;
  user-select: none;
  font-family: sans-serif;
}

.axis-label-y {
  position: absolute;
  top: 2px;
  right: 5px;
  font-size: 10px;
  color: #999;
  pointer-events: none;
  font-weight: bold;
}
</style>