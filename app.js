
const $ = id => document.getElementById(id);
const file = $('audioFile');
file.addEventListener('change', () => {
  if (file.files[0]) {
    $('player').src = URL.createObjectURL(file.files[0]);
    $('fileStatus').textContent = file.files[0].name;
  }
});
$('speed').oninput = e => $('speedOut').value = e.target.value;
$('amount').oninput = e => $('amountOut').value = e.target.value + '%';

let ctx, stream, analyser, source, tuner, raf;
const names = ['דו', 'דו♯', 'רה', 'רה♯', 'מי', 'פה', 'פה♯', 'סול', 'סול♯', 'לה', 'לה♯', 'סי'];
const scales = {
  C: [0,2,4,5,7,9,11], G: [7,9,11,0,2,4,6], D: [2,4,6,7,9,11,1],
  A: [9,11,1,2,4,6,8], F: [5,7,9,10,0,2,4], Am: [9,11,0,2,4,5,7],
  Chromatic: [0,1,2,3,4,5,6,7,8,9,10,11]
};

function detectPitch(buffer, sampleRate) {
  let rms = 0;
  for (let i = 0; i < buffer.length; i++) rms += buffer[i] * buffer[i];
  if (Math.sqrt(rms / buffer.length) < 0.012) return 0;
  let bestLag = 0, bestCorrelation = 0;
  for (let lag = Math.floor(sampleRate / 1000); lag < Math.min(Math.floor(sampleRate / 65), buffer.length - 1); lag++) {
    let correlation = 0;
    for (let i = 0; i < buffer.length - lag; i += 2) correlation += buffer[i] * buffer[i + lag];
    if (correlation > bestCorrelation) { bestCorrelation = correlation; bestLag = lag; }
  }
  return bestLag && bestCorrelation > 0.015 ? sampleRate / bestLag : 0;
}

function nearestScaleNote(midi, scaleName) {
  const allowed = scales[scaleName] || scales.C;
  let nearest = Math.round(midi), distance = Infinity;
  for (let candidate = Math.round(midi) - 12; candidate <= Math.round(midi) + 12; candidate++) {
    if (allowed.includes((candidate % 12 + 12) % 12)) {
      const d = Math.abs(candidate - midi);
      if (d < distance) { distance = d; nearest = candidate; }
    }
  }
  return nearest;
}

function workletSource() {
  return `
    class VocalTuneProcessor extends AudioWorkletProcessor {
      constructor() {
        super();
        this.size = 32768; this.mask = this.size - 1;
        this.ring = new Float32Array(this.size);
        this.writeIndex = 0; this.grains = [];
        this.grainSize = 2048; this.hopSize = 512; this.untilNextGrain = 0;
        this.ratio = 1; this.targetRatio = 1; this.smoothing = 0.12;
        this.port.onmessage = event => {
          const data = event.data || {};
          if (Number.isFinite(data.ratio)) this.targetRatio = Math.max(0.5, Math.min(2, data.ratio));
          if (Number.isFinite(data.smoothing)) this.smoothing = Math.max(0.005, Math.min(0.5, data.smoothing));
        };
      }
      process(inputs, outputs) {
        const input = inputs[0] && inputs[0][0];
        const output = outputs[0] && outputs[0][0];
        if (!output) return true;
        for (let i = 0; i < output.length; i++) {
          const sample = input ? input[i] : 0;
          this.ring[this.writeIndex] = sample;
          this.ratio += (this.targetRatio - this.ratio) * this.smoothing;
          if (this.untilNextGrain <= 0) {
            const back = Math.floor(this.grainSize * this.ratio) + 8;
            this.grains.push({ start: (this.writeIndex - back) & this.mask, phase: 0, ratio: this.ratio });
            this.untilNextGrain = this.hopSize;
          }
          this.untilNextGrain--;
          let mixed = 0, weightSum = 0;
          const active = [];
          for (const grain of this.grains) {
            const phase = grain.phase;
            if (phase < this.grainSize) {
              const window = 0.5 - 0.5 * Math.cos(2 * Math.PI * phase / (this.grainSize - 1));
              const position = grain.start + phase * grain.ratio;
              const index = Math.floor(position), fraction = position - index;
              const a = this.ring[index & this.mask], b = this.ring[(index + 1) & this.mask];
              mixed += (a + (b - a) * fraction) * window;
              weightSum += window; grain.phase++; active.push(grain);
            }
          }
          this.grains = active;
          output[i] = weightSum > 0.0001 ? mixed / weightSum : sample;
          this.writeIndex = (this.writeIndex + 1) & this.mask;
        }
        return true;
      }
    }
    registerProcessor('vocal-tune-processor', VocalTuneProcessor);
  `;
}

function updateTuner(hz) {
  if (!tuner || !hz) return;
  const midi = 69 + 12 * Math.log2(hz / 440);
  const targetMidi = nearestScaleNote(midi, $('key').value);
  const amount = Number($('amount').value) / 100;
  const ratio = Math.pow(2, ((targetMidi - midi) * amount) / 12);
  const speed = Number($('speed').value);
  tuner.port.postMessage({ ratio, smoothing: 0.28 - (speed / 100) * 0.25 });
}

function tick() {
  if (!analyser || !ctx) return;
  const buffer = new Float32Array(analyser.fftSize);
  analyser.getFloatTimeDomainData(buffer);
  const hz = detectPitch(buffer, ctx.sampleRate);
  if (hz > 55 && hz < 1000) {
    const midi = Math.round(69 + 12 * Math.log2(hz / 440));
    $('note').textContent = names[(midi % 12 + 12) % 12];
    $('frequency').textContent = hz.toFixed(1) + ' Hz';
    $('meterFill').style.width = Math.min(100, Math.max(0, hz / 10)) + '%';
    updateTuner(hz);
  } else {
    $('note').textContent = '—';
    $('frequency').textContent = 'לא זוהה תו';
    if (tuner) tuner.port.postMessage({ ratio: 1, smoothing: 0.08 });
  }
  raf = requestAnimationFrame(tick);
}

async function startAutoTune() {
  try {
    if (!navigator.mediaDevices?.getUserMedia) throw new Error('הדפדפן לא תומך בגישה למיקרופון');
    stream = await navigator.mediaDevices.getUserMedia({ audio: {
      echoCancellation: false, noiseSuppression: false, autoGainControl: false, channelCount: 1
    }});
    ctx = new AudioContext();
    await ctx.audioWorklet.addModule(URL.createObjectURL(new Blob([workletSource()], { type: 'application/javascript' })));
    source = ctx.createMediaStreamSource(stream);
    analyser = ctx.createAnalyser(); analyser.fftSize = 4096;
    tuner = new AudioWorkletNode(ctx, 'vocal-tune-processor', {
      numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [1]
    });
    source.connect(analyser);
    source.connect(tuner);
    tuner.connect(ctx.destination);
    $('monitorHint').textContent = 'Auto-Tune פעיל — מומלץ להשתמש באוזניות';
    $('micBtn').disabled = true; $('stopBtn').disabled = false;
    tick();
  } catch (error) {
    console.error(error);
    $('monitorHint').textContent = error.message || 'לא ניתן להפעיל תיקון. בדוק הרשאות מיקרופון.';
    stream?.getTracks().forEach(track => track.stop());
    if (ctx) await ctx.close();
    ctx = stream = analyser = source = tuner = null;
  }
}

function stopAutoTune() {
  cancelAnimationFrame(raf);
  stream?.getTracks().forEach(track => track.stop());
  if (ctx && ctx.state !== 'closed') ctx.close();
  ctx = stream = analyser = source = tuner = null;
  $('micBtn').disabled = false; $('stopBtn').disabled = true;
  $('note').textContent = '—'; $('frequency').textContent = 'ממתין לאות שמע';
  $('meterFill').style.width = '0%'; $('monitorHint').textContent = 'המיקרופון הופסק';
}

$('micBtn').addEventListener('click', startAutoTune);
$('stopBtn').addEventListener('click', stopAutoTune);
$('key').addEventListener('change', () => {
  $('monitorHint').textContent = 'הסולם עודכן — התיקון יפעל לפי הסולם שנבחר';
});
