#version 330 core

uniform vec3 color;

in  vec3 v_normal;
out vec4 FragColor;

void main() {
    // Ha nincs normalis (letiltott attributum -> (0,0,0)), akkor egyszinu rajz:
    // ez kell a tengelyeknek, a racsnak es a feliratoknak.
    if (dot(v_normal, v_normal) < 0.25) {
        FragColor = vec4(color, 1.0);
        return;
    }

    vec3 N = normalize(v_normal);

    // Ket fenyforras a Z-up jelenethez: egy fo feny felulrol-oldalrol, es egy
    // gyenge kitolto feny a masik oldalrol, hogy az arnyekos resz se legyen fekete.
    const vec3 KEY  = normalize(vec3( 0.35,  0.45, 0.82));
    const vec3 FILL = normalize(vec3(-0.45, -0.35, 0.20));

    // "Half-Lambert": a dot-ot [0,1]-re kepezzuk le a szokasos max(0,dot) helyett.
    // Igy a fenytol elfordulo korongok sem esnek egyetlen egyenletes fekete foltba,
    // vagyis a gomb formaja vegig olvashato marad. A negyzetre emeles adja vissza
    // a valosagosabb esest.
    float key  = dot(N, KEY)  * 0.5 + 0.5;
    float fill = dot(N, FILL) * 0.5 + 0.5;

    float lit = 0.25 + 0.62 * key * key + 0.15 * fill;

    // Keskeny csucsfeny: segit elkuloniteni az egymas melletti korongokat.
    vec3 shaded = color * lit + vec3(0.14) * pow(key, 16.0);

    FragColor = vec4(clamp(shaded, 0.0, 1.0), 1.0);
}
