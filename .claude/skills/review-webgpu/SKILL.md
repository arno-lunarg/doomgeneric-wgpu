---
name: review-webgpu
description: Review the learner's WebGPU C code and WGSL in this repo without rewriting it. Use when the learner asks for a review, asks why something fails, or says a tutorial step is done.
---

# Review WebGPU code

Report, do not fix. The learner applies the changes.

## Steps

1. `git diff` and `git status` to find what they wrote since the last commit.
2. Build Debug. Run the affected exe with `tools/capture.ps1` on the default backend and `-vulkan`. Collect `[gpu]` / `[dawn]` lines from stderr.
3. Check the code against the list below.
4. Report findings ranked by severity: file:line, what is wrong, why (Vulkan comparison when it helps), the reference (`webgpu.h:<line>` or spec URL). No replacement code unless asked.
5. If the step is complete, say so in one line and update the curriculum status in `AGENT.md`.

## Checklist

- Structs start from `WGPU_*_INIT`, not `{0}` or field-by-field from zero.
- Every `Create*` / `Get*` returning an object has a matching `Release`, also on early-return and skip paths (surface textures included).
- `WGPUStringView` received from Dawn is printed with an explicit length.
- Callback modes fit the wait mechanism: `WaitAnyOnly` only with `wgpuInstanceWaitAny`; `AllowProcessEvents` needs `wgpuInstanceProcessEvents` somewhere.
- Device lost callback filters `Destroyed` / `CallbackCancelled`.
- Surface texture status handled: reconfigure on `Outdated` / `Lost`; no draw on failure; 0×0 sizes skipped.
- Labels on descriptors.
- Vulkan habits that do not apply: manual barriers or layout thinking, per-frame resource ring buffers where `wgpuQueueWriteBuffer` / `WriteTexture` suffice, assuming multiple queues, assuming mapped memory stays mapped.
- `wgpuQueueWriteTexture`: `bytesPerRow` is the source row pitch (no 256 alignment required, unlike buffer-to-texture copies).
- WGSL: integer textures use `textureLoad`, not samplers; uniformity errors from derivatives / `textureSample` in non-uniform control flow; struct alignment rules for uniform buffers.
- Default limits: anything relying on more than core limits must request it in `requiredLimits`.
