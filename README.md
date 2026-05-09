# DeepstreamAnonymizer
Deepstream pipeline to pixelate people

## Requirements
* NVIDIA DeepStream SDK (tested with 7.x)
* CUDA-enabled GPU
* GStreamer 1.0+
* Properly configured DeepStream environment ($DEEPSTREAM_DIR, plugins, etc.)

## Run

```
make
./app config.yml
```

## Notes

Performance mode can be enabled via environment variable:
```
export NVDS_PERF_MODE=1
```