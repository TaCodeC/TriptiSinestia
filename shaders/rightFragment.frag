#version 330 core

uniform vec2 u_resolution;
uniform float u_time;
uniform float u_flowerDensity;
uniform float u_noiseAmount;
uniform float u_swirlIntensity;

in vec2 TexCoords;
out vec4 FragColor;

#define S(a,b,c) smoothstep(a,b,c)
#define sat(a) clamp(a,0.0,1.0)

// Pseudo-random generator
vec4 N14(float t) {
    return fract(sin(t * vec4(123.0, 104.0, 145.0, 24.0)) * vec4(657.0, 345.0, 879.0, 154.0));
}

const float baseFlowerScale = 8.0;

// Signed distance of sakura petal shape
vec4 sakura(vec2 uv, vec2 id, float blur) {
    vec4 rnd  = N14(mod(id.x,500.0)*5.4 + mod(id.y,500.0)*13.67);

    // ——— escala con variación extra ———
    float extraScale = mix(0.8, 1.2, rnd.y);
    uv *= mix(0.75, 1.3, rnd.y) * extraScale
    * baseFlowerScale * pow(u_flowerDensity, -0.5);

    // ——— movimiento más aleatorio ———
    float speedFactor = mix(0.2, 1.5, rnd.z);
    float dirAngle    = rnd.w * 6.2831853; // 2π
    float t = (u_time + 45.0) * speedFactor;
    float amp = mix(0.3, 1.0, rnd.w);

    uv += vec2(
    cos(dirAngle) * sin(t + rnd.x * 3.14),
    sin(dirAngle) * cos(t + rnd.y * 1.73)
    ) * amp;

    // ——— swirl también aleatorio ———
    float swirlSpeed = mix(-1.5, 1.5, rnd.w);
    float swirl      = u_time * swirlSpeed;

    // resto del cálculo…
    float angle = atan(uv.y, uv.x) + rnd.x * 421.47 + swirl;
    float dist  = length(uv);

    // forma
    float petal  = 1.0 - abs(sin(angle * 2.5));
    float sq     = petal*petal;
    petal        = mix(petal, sq, 0.7);
    float petal2 = 1.0 - abs(sin(angle * 2.5 + 1.5));
    petal       += petal2 * 0.2;
    float sakuraDist = dist + petal * 0.25;

    // sombras y máscara
    float shadow     = S(0.8, 0.2, sakuraDist) * 0.4;
    float sakuraMask = S(0.5+blur, 0.5-blur, sakuraDist);

    // color atenuado
    vec3 petalCol = mix(vec3(1.0,0.6,0.7), vec3(0.7), 0.3)
    + (0.5 - dist) * 0.2;

    // contorno y pistilos
    float outlineMask = S(0.5-blur, 0.5, sakuraDist + 0.045);
    float polar       = angle * 1.9098 + 0.5;
    float pist        = fract(polar) - 0.5;
    float petBlur     = blur * 2.0;
    float barW        = 0.2 - dist * 0.7;
    float pistilBar   = S(-barW, -barW+petBlur, pist)
    * S(barW+petBlur, barW, pist);
    float pistilMask  = S(0.12+blur, 0.12, dist)
    * S(0.05, 0.05+blur, dist);
    float pistilDot   = S(0.1+petBlur, 0.1-petBlur,
    length(vec2(pist*0.1,dist)
    - vec2(0,0.16))*9.0);

    outlineMask += pistilMask * pistilBar + pistilDot;

    // mezcla
    vec3 c = mix(petalCol, vec3(1.0,0.3,0.3), sat(outlineMask)*0.5);
    c = mix(vec3(0.2,0.2,0.8)*shadow, c, sakuraMask);

    sakuraMask = sat(sakuraMask + shadow);
    return vec4(c * sakuraMask, sakuraMask);
}

// blending con alpha premultiplicado
vec4 blend(vec4 src, vec4 dst) {
    vec3 rgb = dst.rgb * (1.0 - src.a)
    + src.rgb * src.a;
    float a = src.a + dst.a * (1.0 - src.a);
    return vec4(rgb, a);
}


// Crea una capa de flores repetidas
vec4 layer(vec2 uv, float blur) {
    vec2 id = floor(uv);
    vec2 fu = fract(uv) - 0.5;
    vec4 acc = vec4(0);
    for(int y = -1; y <= 1; ++y) {
        for(int x = -1; x <= 1; ++x) {
            vec2 off = vec2(x, y);
            vec4 sak = sakura(fu - off, id + off, blur);
            acc = blend(sak, acc);
        }
    }
    return acc;
}

// Fondo Nguyen2007 con control de ruido
vec3 backgroundNguyen(vec2 fragCoord) {
    vec2 v = u_resolution;
    vec2 u =  0.2 * (fragCoord * 2.0 - v) / v.y;

    vec4 z = vec4(1,2,3,0), o = vec4(0);
    float a = 0.01, t = u_time;
    for(float i = 0.0; i < 19.0; i++) {
        o += (.90 + cos(z + t))
        / length((1.0 + i * dot(v, v)) * sin(1.5 * u / (0.5 - dot(u,u)) - 9.0 * u.yx + t));
        v = cos(++t - 7.0 * u * pow(a += .03, i)) - 5.0 * u;
    u += tanh(u_swirlIntensity  * dot(u *= mat2(cos(i + 0.02 /* <- Recordar agregar alteracion por uniform*/* t - vec4(0,11,33,0))), u)
    * cos(100.0 * u.yx + t))/200.0
    + 0.2 * a * u
    + cos(1.0 / exp(dot(o,o) / 100.0) + t)/300.0;
    }
    vec3 noiseCol = vec3(25.6) / (min(o.rgb,13.0) + 164.0 / o.rgb) - dot(u,u) /200;
    vec3 baseColor = mix(vec3(0.3,0.3,1.0), vec3(1.0), fragCoord.y / u_resolution.y);
    // --- Nueva sección: máscara de bordes ---
    // distancia normalizada al centro [0 = centro, 1 = esquina]
    float distToCenter = length((fragCoord - 0.5 * v) / v);
    // ramp-up suave del ruido entre 0.6 y 0.9 de distToCenter
    float edgeMask = smoothstep(0.2, 20., distToCenter);

    // mezclamos
    return mix(
        baseColor,
        noiseCol,
        u_noiseAmount * edgeMask
    );
}

void main() {
    if(u_resolution.y == 0.0) { FragColor = vec4(0); return; }

    vec2 nom = TexCoords;
    if (u_flowerDensity <= 0.1) {
        vec2 fragLocal = TexCoords * u_resolution;
        vec3 bg = backgroundNguyen(fragLocal);
        FragColor = vec4(bg, 1.0);
        return;
    }
    vec2 p = nom - 0.5;
    p.x *= u_resolution.x / u_resolution.y;
    p.y += u_time * 0.1;
    p.x -= u_time * 0.03 + sin(u_time) * 0.1;

    // Regula cantidad: más densidad = más flores
    p *= u_flowerDensity;

    float blur = abs(nom.y -1.);
    blur = blur * blur * 2.0 * 0.15;

    vec2 fragLocal = nom * u_resolution;       // nom = TexCoords
    vec3 col     = backgroundNguyen(fragLocal);

    vec4 L1 = layer(p,               0.015 + blur);
    vec4 L2 = layer(p * 1.5 + vec2(124.5,89.3), 0.05 + blur);
    L2.rgb *= mix(0.7,0.95,nom.y);
    vec4 L3 = layer(p * 2.3 + vec2(463.5,-987.3), 0.08 + blur);
    L3.rgb *= mix(0.55,0.85,nom.y);

    col = blend(L3, vec4(col,1.0)).rgb;
    col = blend(L2, vec4(col,1.0)).rgb;
    col = blend(L1, vec4(col,1.0)).rgb;

    FragColor = vec4(col, 1.0);
}