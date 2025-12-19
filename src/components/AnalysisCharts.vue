<script setup>
import { ref, computed, onMounted, onUnmounted } from "vue";

const props = defineProps({
  data: {
    type: Object,
    required: true,
  },
  range: {
    type: Object,
    required: true,
  },
});

const container = ref(null);
const width = ref(300);
const swrHeight = 150;
const smithSize = 300;
const hoverIndex = ref(-1);

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

const minFreq = computed(() => props.range.start);
const maxFreq = computed(() => props.range.end);
const freqRange = computed(() => maxFreq.value - minFreq.value || 1);

const maxDataSwr = computed(() => {
  if (!props.data.values.length) return 5;
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
  if (den === 0) return { x: smithCx, y: smithCy };

  const u = (rn ** 2 + xn ** 2 - 1) / den;
  const v = (2 * xn) / den;
  return {
    x: smithCx + u * smithR,
    y: smithCy - v * smithR,
  };
};

const swrPoints = computed(() => {
  return props.data.labels.map((l, i) => ({
    x: mapFreqX(parseFloat(l)),
    y: mapSwrY(props.data.values[i]),
  }));
});

const swrPath = computed(() => getSmoothPath(swrPoints.value));

const smithPoints = computed(() => {
  return props.data.impedance.map((z) => getSmithPt(z.r, z.i));
});

const smithPath = computed(() => getSmoothPath(smithPoints.value));

const smithGrid = [0, 0.5, 1.0, 3.0].map((rVal) => {
  const u = rVal / (1 + rVal);
  const r = 1 / (1 + rVal);
  return { cx: smithCx + u * smithR, cy: smithCy, r: r * smithR };
});

const onSwrHover = (e) => {
  const rect = e.currentTarget.getBoundingClientRect();
  const x = e.clientX - rect.left;
  const freq = minFreq.value + (x / width.value) * freqRange.value;

  let closestIdx = -1;
  let minDiff = Infinity;

  props.data.labels.forEach((l, i) => {
    const f = parseFloat(l);
    const diff = Math.abs(f - freq);
    if (diff < minDiff) {
      minDiff = diff;
      closestIdx = i;
    }
  });

  hoverIndex.value = closestIdx;
};

const onSmithHover = (e) => {
  const rect = e.currentTarget.getBoundingClientRect();
  const scaleX = smithSize / rect.width;
  const scaleY = smithSize / rect.height;
  const mouseX = (e.clientX - rect.left) * scaleX;
  const mouseY = (e.clientY - rect.top) * scaleY;

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

const onLeave = () => {
  hoverIndex.value = -1;
};

const activeInfo = computed(() => {
  if (hoverIndex.value === -1 || !props.data.labels[hoverIndex.value])
    return null;
  const i = hoverIndex.value;
  const freq = parseFloat(props.data.labels[i]);
  const swr = props.data.values[i];
  const z = props.data.impedance[i];

  return {
    freq: freq.toFixed(1),
    swr: swr.toFixed(3),
    zReal: z.r.toFixed(1),
    zImag: z.i.toFixed(1),
    swrPt: { x: mapFreqX(freq), y: mapSwrY(swr) },
    smithPt: getSmithPt(z.r, z.i),
  };
});

const xTicks = computed(() => {
  const steps = 5;
  const arr = [];
  for (let i = 0; i <= steps; i++) {
    const f = minFreq.value + (i / steps) * freqRange.value;
    arr.push({ x: (i / steps) * width.value, val: f.toFixed(0) });
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
    arr.push({ y: mapSwrY(val), val: val.toFixed(1) });
  }
  return arr;
});
</script>

<template>
  <div class="charts-root" ref="container">
    <!-- Info Bar -->
    <div class="info-bar">
      <div v-if="activeInfo" class="info-content">
        <span class="info-item">Freq: {{ activeInfo.freq }} MHz</span>
        <span class="info-item">SWR: {{ activeInfo.swr }}</span>
        <span class="info-item"
          >Z: {{ activeInfo.zReal }}{{ activeInfo.zImag >= 0 ? "+" : ""
          }}{{ activeInfo.zImag }}j</span
        >
      </div>
      <span v-else style="color: #ccc">--</span>
    </div>

    <!-- SWR Chart -->
    <div class="chart-wrapper">
      <div class="axis-label-y">SWR</div>
      <svg
        class="chart-svg"
        :viewBox="`0 0 ${width} ${swrHeight}`"
        preserveAspectRatio="none"
        @mousemove="onSwrHover"
        @mouseleave="onLeave"
      >
        <!-- Grid -->
        <g class="grid">
          <line
            v-for="t in yTicks"
            :key="t.val"
            x1="0"
            :y1="t.y"
            :x2="width"
            :y2="t.y"
          />
          <line
            v-for="t in xTicks"
            :key="t.val"
            :x1="t.x"
            y1="0"
            :x2="t.x"
            :y2="swrHeight"
          />
        </g>

        <!-- Data Line -->
        <path :d="swrPath" fill="none" stroke="#000" stroke-width="1.5" />

        <!-- Highlight Point -->
        <circle
          v-if="activeInfo"
          :cx="activeInfo.swrPt.x"
          :cy="activeInfo.swrPt.y"
          r="4"
          fill="#000"
        />

        <!-- Axis Labels -->
        <g class="axis-text">
          <!-- 
             Y轴标签：
             v-if="i !== 0" : 隐藏底部的 1.0 标签，防止与频率重叠
          -->
          <text
            v-for="(t, i) in yTicks"
            :key="'y' + t.val"
            v-show="i !== 0"
            x="2"
            :y="t.y - 3"
          >
            {{ t.val }}
          </text>

          <!-- 
             X轴标签：
             text-anchor 逻辑: 
             第一个点(i==0): start (左对齐，不超出左边界)
             最后一个点(i==len-1): end (右对齐，不超出右边界)
             中间点: middle (居中)
          -->
          <text
            v-for="(t, i) in xTicks"
            :key="'x' + t.val"
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

    <!-- Smith Chart -->
    <div class="chart-wrapper smith-wrapper">
      <div class="axis-label-y" style="top: 5px; left: 5px">Smith</div>
      <svg
        class="smith-svg"
        :viewBox="`0 0 ${smithSize} ${smithSize}`"
        @mousemove="onSmithHover"
        @mouseleave="onLeave"
      >
        <g class="smith-grid">
          <circle
            :cx="smithCx"
            :cy="smithCy"
            :r="smithR"
            fill="none"
            stroke="#ddd"
          />
          <line
            :x1="smithCx - smithR"
            :y1="smithCy"
            :x2="smithCx + smithR"
            :y2="smithCy"
            stroke="#ddd"
          />
          <circle
            v-for="(c, i) in smithGrid"
            :key="i"
            :cx="c.cx"
            :cy="c.cy"
            :r="c.r"
            fill="none"
            stroke="#eee"
          />
        </g>
        <path :d="smithPath" fill="none" stroke="#000" stroke-width="1.5" />
        <circle
          v-if="activeInfo"
          :cx="activeInfo.smithPt.x"
          :cy="activeInfo.smithPt.y"
          r="4"
          fill="#000"
        />
      </svg>
    </div>
  </div>
</template>

<style scoped>
.charts-root {
  display: flex;
  flex-direction: column;
  gap: 20px;
  width: 100%;
}

.info-bar {
  font-size: 12px;
  overflow: hidden;
  color: #000;
}

.info-content {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  align-items: center;
}

.info-item {
  white-space: nowrap;
}

.chart-wrapper {
  position: relative;
  border: 1px solid #eee;
  background: #fff;
  padding: 0;
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
}

.smith-svg {
  width: 100%;
  max-width: 300px;
  aspect-ratio: 1/1;
  display: block;
  cursor: crosshair;
}

.grid line {
  stroke: #f0f0f0;
  stroke-width: 1;
}
.smith-grid circle,
.smith-grid line {
  stroke-width: 1;
  vector-effect: non-scaling-stroke;
}

.axis-text text {
  font-size: 10px;
  fill: #666;
  user-select: none;
}

.axis-label-y {
  position: absolute;
  top: 2px;
  right: 5px;
  font-size: 10px;
  color: #ccc;
  pointer-events: none;
}
</style>