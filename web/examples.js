// StimGen 2 -- SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 Michele Giugliano
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// examples.js -- the examples of the planner's Examples menu.  Each one is
// a complete StimGen 2 description; tests/test_web.mjs renders every one in
// the browser and checks it with the native sg.  Waveforms use the unit
// chosen in the planner (pA by default); stimuli state their own units.

export const EXAMPLES = [
  // ------------------------------------------------------------ basics
  ["Basics", "A current step",
`# a 1 s step of 300 pA between two pauses
500ms  dc(0)
1s     dc(300)
500ms  dc(0)
`],
  ["Basics", "Hyperpolarising and depolarising steps",
`# one trial, two steps: input resistance and firing
500ms  dc(0)
@hyper
1s     dc(-100)
1s     dc(0)
@depol
1s     dc(250)
500ms  dc(0)
`],
  ["Basics", "A brief pulse (one spike)",
`# 2 ms at 2 nA: usually one action potential
200ms  dc(0)
@pulse
2ms    dc(2000)
300ms  dc(0)
`],
  ["Basics", "Ramp to find the rheobase",
`# a slow ramp: the current at the first spike is the rheobase
500ms  dc(0)
5s     ramp(from=0, to=500)
500ms  dc(0)
`],
  ["Basics", "Sag (Ih) test",
`# a long hyperpolarising step: watch the voltage sag back
500ms  dc(0)
2s     dc(-200)
1s     dc(0)
`],
  ["Basics", "Membrane time constant",
`# small negative pulses, repeated: average them to fit tau
200ms dc(0)
repeat 20 {
    100ms dc(0)
    100ms dc(-30)
}
`],
  ["Basics", "Ramp up, hold, ramp down (prev)",
`# each ramp starts where the previous segment ended (from=prev, the default)
500ms dc(0)
1s    ramp(to=200)
2s    dc(prev)
1s    ramp(to=0)
500ms dc(0)
`],

  // ------------------------------------------------------------ waves
  ["Waves", "Generators, one after the other",
`# one segment per generator
300ms ramp(from=0, to=100)
300ms sine(amp=50, freq=10Hz, offset=100)
300ms square(amp=50, freq=10Hz, duty=25%, offset=100)
300ms chirp(amp=50, f0=2Hz, f1=40Hz, offset=100)
300ms pulses(amp=100, rate=20Hz, width=2ms)
300ms biexp(amp=100, tau_rise=2ms, tau_decay=20ms, delay=20ms)
`],
  ["Waves", "ZAP: impedance profile (linear chirp)",
`# a frequency sweep from 0.5 to 40 Hz (Puil et al. 1986)
1s   dc(0)
20s  chirp(amp=30, f0=0.5Hz, f1=40Hz)
1s   dc(0)
`],
  ["Waves", "Exponential chirp (equal time per octave)",
`1s   dc(0)
20s  chirp(amp=30, f0=0.5Hz, f1=64Hz, law=exp)
1s   dc(0)
`],
  ["Waves", "Sinusoids at several frequencies",
`# 2 s at each frequency, on a depolarising offset
2s sine(amp=40, freq=1Hz,  offset=100)
2s sine(amp=40, freq=4Hz,  offset=100)
2s sine(amp=40, freq=16Hz, offset=100)
2s sine(amp=40, freq=64Hz, offset=100)
`],
  ["Waves", "Square, saw, triangle",
`1s square(amp=50, freq=4Hz, duty=50%)
1s saw(amp=50, freq=4Hz)
1s saw(amp=50, freq=4Hz, duty=0)
1s triangle(amp=50, freq=4Hz)
`],
  ["Waves", "Phase-locked oscillation, gated (clock=global)",
`# the sine keeps its phase across the pauses, as if it had kept running
1s sine(amp=50, freq=5Hz, clock=global)
1s dc(0)
1s sine(amp=50, freq=5Hz, clock=global)
1s dc(0)
1s sine(amp=50, freq=5Hz, clock=global)
`],

  // ------------------------------------------------------------ synaptic
  ["Synaptic", "One EPSC-like current",
`# difference of exponentials, peak 30 pA
50ms   dc(0)
200ms  biexp(amp=-30, tau_rise=1ms, tau_decay=8ms)
`],
  ["Synaptic", "Train of EPSCs at 20 Hz",
`200ms dc(0)
@train
500ms pulses(amp=-40, rate=20Hz, shape=biexp, tau_rise=1ms, tau_decay=10ms)
300ms dc(0)
`],
  ["Synaptic", "Poisson synaptic barrage",
`# random EPSCs at 80 Hz on average, with a 2 ms dead time
3s pulses(amp=-20, rate=80Hz, shape=biexp, tau_rise=0.5ms, tau_decay=5ms, timing=poisson, dead=2ms)
`],
  ["Synaptic", "Paired pulses (50 ms apart)",
`# two 1 ms pulses at explicit times
300ms pulses(amp=2000, times=[50ms, 100ms], width=1ms)
`],
  ["Synaptic", "Alpha-function inputs, one by one",
`repeat 5 {
    150ms alpha(amp=40, tau=5ms, delay=20ms)
}
`],

  // ------------------------------------------------------------ noise
  ["Noise", "Noise: new each time, and frozen",
`1s   dc(0)
2s   ou(mean=100, sd=50, tau=5ms)            # no seed: new noise each time
500ms dc(0)
2s   ou(mean=100, sd=50, tau=5ms, seed=17)   # seed=17: always the same
`],
  ["Noise", "Correlation time: 1, 5 and 20 ms",
`# the same mean and SD, slower and slower fluctuations
2s ou(mean=0, sd=40, tau=1ms)
2s ou(mean=0, sd=40, tau=5ms)
2s ou(mean=0, sd=40, tau=20ms)
`],
  ["Noise", "White, pink and brown noise",
`# power spectral density ~ 1/f^alpha
2s cnoise(mean=0, sd=30, alpha=0)
2s cnoise(mean=0, sd=30, alpha=1)
2s cnoise(mean=0, sd=30, alpha=2)
`],
  ["Noise", "In vivo-like: noise on a slow oscillation",
`10s ou(mean=0, sd=40, tau=3ms) + sine(amp=60, freq=1Hz, offset=100)
`],
  ["Noise", "A weak sine hidden in noise",
`# signal detection: can the neuron follow 8 Hz?
10s sine(amp=10, freq=8Hz) + ou(mean=50, sd=30, tau=5ms)
`],
  ["Noise", "Uniform white noise",
`2s unoise(mean=0, sd=30)
`],

  // ------------------------------------------------------------ combining
  ["Combining", "Envelope (block × generator)",
`{ 1s ramp(0, 1) ; 3s dc(1) ; 1s ramp(1, 0) } * sine(amp=80, freq=8Hz)
`],
  ["Combining", "Amplitude modulation",
`5s (1 + 0.5*sine(1, 2Hz)) * sine(amp=80, freq=40Hz)
`],
  ["Combining", "Noise whose size changes slowly",
`10s ou(mean=0, sd=1, tau=5ms) * (30 + 20*sine(1, 0.5Hz))
`],
  ["Combining", "Rectified sine, clipped noise",
`3s abs(sine(amp=100, freq=2Hz))
3s pos(sine(amp=100, freq=2Hz))
3s clip(ou(mean=0, sd=100, tau=5ms), -80, 80)
`],
  ["Combining", "Steps with added noise",
`# a staircase, the same noise added to all of it
1s dc(0)
{
    2s dc(100) ; 2s dc(200) ; 2s dc(300)
} + ou(mean=0, sd=30, tau=5ms, seed=3)
1s dc(0)
`],

  // ------------------------------------------------------------ stimuli
  ["Stimuli", "Two cells and a camera",
`sg 2 stimulus
rate 20kHz

channel pre unit=pA {
    1s     dc(0)
    @train
    500ms  pulses(amp=2000, rate=20Hz, width=1ms)
    1s     dc(0)
}

channel post unit=pA {
    2.5s   dc(-50) + ou(0, 10, 3ms)
}

digital camera {
    2.5s   pulses(1, 50Hz, width=1ms)
}
`],
  ["Stimuli", "Voltage clamp: a step from -70 mV",
`sg 2 stimulus
channel Vcmd unit=mV rest=-70 {
    100ms  dc(-70)
    @step
    200ms  dc(-10)
    100ms  dc(-70)
}
`],
  ["Stimuli", "Dynamic clamp: excitatory and inhibitory conductances",
`sg 2 stimulus
# two conductance waveforms; pos() keeps them non-negative
channel ge unit=nS { 5s pos(ou(mean=10, sd=4, tau=3ms)) }
channel gi unit=nS { 5s pos(ou(mean=20, sd=8, tau=10ms)) }
`],
  ["Stimuli", "Optogenetics: light pulses and a trigger",
`sg 2 stimulus
channel led unit=V {
    500ms dc(0)
    @light
    1s    pulses(amp=5, rate=10Hz, width=5ms)
    500ms dc(0)
}
digital trig { 1ms dc(1) ; 1999ms dc(0) }
marker analysis at 0.4s
`],
  ["Stimuli", "The same noise on two channels (copy)",
`sg 2 stimulus
channel a unit=pA { 2s ou(mean=0, sd=30, tau=5ms) }
channel b unit=pA copy a
channel c unit=pA { 2s ou(mean=0, sd=30, tau=5ms) }   # a different realisation
`],

  // ------------------------------------------------------------ protocols
  ["Protocols", "A family of steps",
`sg 2 protocol
sweep    amp = from -300pA to 300pA step 50pA
repeat   3
order    shuffled-blocks
period   5s

stimulus {
    channel Iinj unit=pA {
        500ms dc(0)
        @step
        1s    dc($amp)
        500ms dc(0)
    }
}
`],
  ["Protocols", "f–I curve",
`sg 2 protocol
sweep  amp = from 0pA to 500pA step 50pA
period 4s

stimulus {
    channel Iinj unit=pA {
        500ms dc(0)
        2s    dc($amp)
        500ms dc(0)
    }
}
`],
  ["Protocols", "Frozen noise: spike-time reliability",
`sg 2 protocol
repeat 10
noise  per-condition       # every repetition: the same realisation
gap    1s

stimulus {
    channel Iinj unit=pA {
        500ms dc(0)
        3s    ou(mean=150, sd=80, tau=5ms)
        500ms dc(0)
    }
}
`],
  ["Protocols", "Noise amplitudes, each one frozen",
`sg 2 protocol
sweep  sd = [20pA, 40pA, 80pA]
repeat 5
order  shuffled
noise  per-condition
period 4s

stimulus {
    channel Iinj unit=pA {
        500ms dc(0)
        2s    ou(mean=100, sd=$sd, tau=5ms)
        500ms dc(0)
    }
}
`],
  ["Protocols", "Sinusoids from 1 to 64 Hz",
`sg 2 protocol
sweep  f = logspace(1Hz, 64Hz, 7)
period 5s

stimulus {
    500ms dc(0)
    3s    sine(amp=40, freq=$f, offset=100)
    500ms dc(0)
}
`],
  ["Protocols", "Paired-pulse ratio at several intervals",
`sg 2 protocol
sweep  isi = [20ms, 50ms, 100ms, 200ms]
repeat 3
order  shuffled-blocks
period 3s

stimulus {
    channel pre unit=pA {
        @first
        400ms pulses(amp=2000, times=[50ms, $(50ms + isi)], width=1ms)
    }
}
`],
  ["Protocols", "Steps × durations (two sweeps, let)",
`sg 2 protocol
sweep amp = [100pA, 200pA, 400pA]
sweep dur = [250ms, 500ms]
let   charge = amp * dur          # derived (not substituted: a charge has no unit)
period 3s

stimulus {
    500ms dc(0)
    $dur  dc($amp)
    500ms dc(0)
}
`],
  ["Protocols", "Pulses of constant charge (tuples)",
`sg 2 protocol
sweep (amp, width) = [(4000pA, 0.5ms), (2000pA, 1ms), (1000pA, 2ms)]
repeat 2

stimulus {
    100ms dc(0)
    200ms pulses(amp=$amp, times=[50ms], width=$width)
}
`],
  ["Protocols", "Voltage clamp I–V",
`sg 2 protocol
sweep  v = from -100mV to 40mV step 20mV
period 2s

stimulus {
    channel Vcmd unit=mV rest=-70 {
        100ms dc(-70)
        @test
        200ms dc($v)
        100ms dc(-70)
    }
}
`],
];
