# RNNoise source record

- Project: Xiph.Org RNNoise
- Upstream: https://github.com/xiph/rnnoise
- Release: v0.2
- Signed release commit: `904a876`
- Release archive: https://github.com/xiph/rnnoise/releases/download/v0.2/rnnoise-0.2.tar.gz
- Archive SHA-256: `90FCE4B00B9FF24C08DBFE31B82FFD43BAE383D85C5535676D28B0A2B11C0D37`
- License: BSD-3-Clause; see `COPYING`
- Local source modifications: none
- Local build integration: XiE supplies only `CMakeLists.txt`. On Windows x64 it
  selects the ABI-baseline SSE2 path through RNNoise's existing
  `OPUS_X86_MAY_HAVE_SSE2` compatibility macro; AVX/AVX2 remain disabled.

The official release archive includes the default generated model in
`src/rnnoise_data.c`; XiE statically links it and does not download or load a
model at runtime.
