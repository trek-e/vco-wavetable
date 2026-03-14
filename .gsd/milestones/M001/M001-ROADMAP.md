# M001: Migration

**Vision:** A polyphonic digital wavetable oscillator module for VCV Rack, inspired by the legendary PPG Wave synthesizers of the 1980s.

## Success Criteria


## Slices

- [x] **S01: Dsp Foundation** `risk:medium` `depends:[]`
  > After this: unit tests prove dsp-foundation works
- [x] **S02: Polyphonic Routing** `risk:medium` `depends:[S01]`
  > After this: unit tests prove Polyphonic Routing works
- [x] **S03: Modulation** `risk:medium` `depends:[S02]`
  > After this: unit tests prove Modulation works
- [x] **S04: User Wavetables** `risk:medium` `depends:[S03]`
  > After this: unit tests prove User Wavetables works
- [ ] **S05: Panel and Submission** `risk:medium` `depends:[S04]`
  > After this: unit tests prove Panel and Submission works
