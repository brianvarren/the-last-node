// Evaluate this file from SuperCollider to prepare the polyphonic sampler engine.
// 1. Open SuperCollider.
// 2. Evaluate this entire file (Cmd/Ctrl + Enter) to boot the server,
//    load buffers, define the synth, and register OSC listeners.
// 3. Use the Python TUI (loop_sampler.py) to send note on/off events and tweak ADSR.

(
s.waitForBoot({
    var bufferPath = "/srv/storage/Dropbox/_reaktor/Instruments/Brutalisk/default samples/bass ebow lo.wav";
    var sampleKey = "plasma";

    // Reduce scheduling latency so OSC-triggered synths fire immediately.
    s.latency = 0.01;

    ~loopSamplerCleanup = {
        OSCdef(\loopSamplerNoteOn).free;
        OSCdef(\loopSamplerNoteOff).free;
        OSCdef(\loopSamplerParam).free;
        OSCdef(\loopSamplerLoop).free;
        ~loopSamplerNoteOffFunc = nil;
        if(~loopSamplerVoices.notNil, {
            ~loopSamplerVoices.values.do { |synth| synth.set(\gate, 0) };
            ~loopSamplerVoices = nil;
        });
        if(~loopSamplerVoiceOrder.notNil) { ~loopSamplerVoiceOrder = nil; };
        if(~loopSamplerBufs.notNil) {
            ~loopSamplerBufs.do { |key, buf| buf.free };
            ~loopSamplerBufs = nil;
        };
        ~loopSamplerADSR = nil;
        ~loopSamplerLoop = nil;
        ~loopSamplerFixLoop = nil;
    };

    ~loopSamplerCleanup.();

    Buffer.read(s, bufferPath, action: { |buffer|
        var channels = buffer.numChannels;

        SynthDef.new(\loopSamplerPlayer, {
            |out = 0, buf = 0, gate = 1, amp = 0.5, rate = 1.0,
            atk = 0.01, dec = 0.1, sus = 0.7, rel = 0.5,
            start = 0.0, end = 1.0, revision = 0|
            var frames, startFrame, endFrame, rateScale, resetTrig, phase, sig, env;

            frames = BufFrames.kr(buf);
            rateScale = BufRateScale.kr(buf) * rate;
            startFrame = (start.clip(0.0, 1.0) * (frames - 1)).max(0.0);
            endFrame = (end.clip(0.0, 1.0) * (frames - 1)).max(startFrame + 1.0);
            resetTrig = K2A.ar(Trig1.kr(Changed.kr(revision), 0.001));
            phase = Phasor.ar(
                trig: resetTrig,
                rate: rateScale,
                start: startFrame,
                end: endFrame,
                resetPos: startFrame
            );
            phase = phase.clip(startFrame, endFrame);
            sig = BufRd.ar(channels, buf, phase, interpolation: 4);
            env = EnvGen.kr(
                Env.adsr(atk, dec, sus, rel),
                gate,
                levelScale: amp,
                doneAction: 2
            );
            Out.ar(out, sig * env);
        }).add;

        ~loopSamplerBufs = Dictionary.new;
        ~loopSamplerBufs[sampleKey.asSymbol] = buffer;

        ~loopSamplerVoices = IdentityDictionary.new;
        ~loopSamplerVoiceOrder = List.new;
        ~loopSamplerMaxVoices = 8;
        ~loopSamplerADSR = (
            atk: 0.01,
            dec: 0.1,
            sus: 0.7,
            rel: 0.5
        );

        ~loopSamplerLoop = (
            start: 0.0,
            end: 1.0,
            revision: 0,
            minSpan: 0.02
        );

        ~loopSamplerFixLoop = { |start, end|
            var span = ~loopSamplerLoop[\minSpan];
            start = start.clip(0.0, 1.0);
            end = end.clip(0.0, 1.0);
            if((end - start) < span, {
                end = (start + span).clip(0.0, 1.0);
                start = (end - span).clip(0.0, 1.0 - span);
            });
            [start, end]
        };

        ~loopSamplerNoteOffFunc = { |path, note, addr, port|
            var key = (path ++ "|" ++ note.asString).asSymbol;
            var synth = ~loopSamplerVoices.removeAt(key);
            if(synth.notNil) {
                synth.set(\gate, 0);
                ~loopSamplerVoiceOrder.remove(key);
                ("[LoopSampler] Note off % note % (voices: %/% )")
                    .format(path, note, ~loopSamplerVoiceOrder.size, ~loopSamplerMaxVoices)
                    .postln;
            };
        };

        OSCdef.new(
            key: \loopSamplerNoteOn,
            func: {
                |msg, time, addr, port|
                var path, note, vel, bufRef, key, rate, synth, loopStart, loopEnd, loopRevision;

                path = msg[1].asString;
                note = msg[2].asInteger;
                vel = msg[3].asFloat.clip(0.0, 1.0);
                bufRef = ~loopSamplerBufs[path.asSymbol];

                if(bufRef.isNil) {
                    ("[LoopSampler] Unknown buffer key % from %:%").format(path, addr, port).warn;
                    ^nil
                };

                if(vel <= 0.0001) {
                    ~loopSamplerNoteOffFunc.(path, note, addr, port);
                    ^nil
                };

                key = (path ++ "|" ++ note.asString).asSymbol;

                if(~loopSamplerVoices[key].notNil) {
                    ~loopSamplerVoices[key].set(\gate, 0);
                    ~loopSamplerVoices.removeAt(key);
                    ~loopSamplerVoiceOrder.remove(key);
                };

                if(~loopSamplerVoiceOrder.size >= ~loopSamplerMaxVoices) {
                    var stealKey = ~loopSamplerVoiceOrder.removeAt(0);
                    var stealSynth = ~loopSamplerVoices.removeAt(stealKey);
                    if(stealSynth.notNil) { stealSynth.set(\gate, 0); };
                };

                rate = (note - 60).midiratio;
                loopStart = ~loopSamplerLoop[\start];
                loopEnd = ~loopSamplerLoop[\end];
                loopRevision = ~loopSamplerLoop[\revision];
                synth = Synth.new(
                    \loopSamplerPlayer,
                    [
                        \buf, bufRef.bufnum,
                        \rate, rate,
                        \amp, vel,
                        \atk, ~loopSamplerADSR[\atk],
                        \dec, ~loopSamplerADSR[\dec],
                        \sus, ~loopSamplerADSR[\sus],
                        \rel, ~loopSamplerADSR[\rel],
                        \start, loopStart,
                        \end, loopEnd,
                        \revision, loopRevision,
                        \gate, 1
                    ],
                    target: s.defaultGroup,
                    addAction: \addToTail
                );

                ~loopSamplerVoices[key] = synth;
                ~loopSamplerVoiceOrder.add(key);

                ("[LoopSampler] Note on % note % vel % (voices: %/% )")
                    .format(path, note, vel.round(0.001), ~loopSamplerVoiceOrder.size, ~loopSamplerMaxVoices)
                    .postln;
            },
            path: '/note_on',
            recvPort: 57120
        );

        OSCdef.new(
            key: \loopSamplerNoteOff,
            func: {
                |msg, time, addr, port|
                var path = msg[1].asString;
                var note = msg[2].asInteger;
                ~loopSamplerNoteOffFunc.(path, note, addr, port);
            },
            path: '/note_off',
            recvPort: 57120
        );

        OSCdef.new(
            key: \loopSamplerParam,
            func: {
                |msg, time, addr, port|
                var param, value;
                param = msg[1].asString;
                value = msg[2].asFloat;

                switch(param,
                    "attack", {
                        ~loopSamplerADSR[\atk] = value.clip(0.001, 2.0);
                    },
                    "decay", {
                        ~loopSamplerADSR[\dec] = value.clip(0.001, 4.0);
                    },
                    "sustain", {
                        ~loopSamplerADSR[\sus] = value.clip(0.0, 1.0);
                    },
                    "release", {
                        ~loopSamplerADSR[\rel] = value.clip(0.005, 8.0);
                    },
                    {
                        ("[LoopSampler] Unknown param % from %:%").format(param, addr, port).warn;
                        ^nil;
                    }
                );

                ~loopSamplerVoices.values.do { |synth|
                    synth.set(
                        \atk, ~loopSamplerADSR[\atk],
                        \dec, ~loopSamplerADSR[\dec],
                        \sus, ~loopSamplerADSR[\sus],
                        \rel, ~loopSamplerADSR[\rel]
                    );
                };

                ("[LoopSampler] Param % -> %").format(param, value.round(0.004)).postln;
            },
            path: '/param',
            recvPort: 57120
        );

        OSCdef.new(
            key: \loopSamplerLoop,
            func: {
                |msg, time, addr, port|
                var start, end, revision;

                start = msg[1] ? ~loopSamplerLoop[\start];
                end = msg[2] ? ~loopSamplerLoop[\end];

                #start, end = ~loopSamplerFixLoop.(start, end);

                ~loopSamplerLoop[\start] = start;
                ~loopSamplerLoop[\end] = end;
                ~loopSamplerLoop[\revision] = ~loopSamplerLoop[\revision] + 1;
                revision = ~loopSamplerLoop[\revision];

                ~loopSamplerVoices.values.do { |synth|
                    synth.set(
                        \start, start,
                        \end, end,
                        \revision, revision
                    );
                };

                ("[LoopSampler] Loop start % end % (rev %)")
                    .format(start.round(0.001), end.round(0.001), revision)
                    .postln;
            },
            path: '/loopSampler/loop',
            recvPort: 57120
        );

        ("[LoopSampler] Ready. Buffer '%' loaded (% channels). Voices capped at %")
            .format(bufferPath, buffer.numChannels, ~loopSamplerMaxVoices)
            .postln;
    });
});
)

CmdPeriod.doOnce({ ~loopSamplerCleanup.() });

// Manual cleanup helper:
// ~loopSamplerCleanup.();
