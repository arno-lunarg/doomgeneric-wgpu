@group(0) @binding(0) var doom_tex: texture_2d<f32>;
@group(0) @binding(1) var doom_sampler: sampler;

 struct VertexOutput {
   @builtin(position) pos: vec4<f32>,
   @location(0) uv: vec2f,
 }

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOutput {    
    var out: VertexOutput;
    let uv = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
    out.pos = vec4f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    out.uv = uv;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return vec4f(textureSample(doom_tex, doom_sampler, in.uv).rgb, 1.0);
}
