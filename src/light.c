#include "light.h"

#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stddef.h>

static float Clamp255(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 255.0f) return 255.0f;
    return value;
}

void Light_Init(LightEnv *light)
{
    light->direction = Vector3Normalize((Vector3){ 0.45f, -0.75f, 0.48f });
    light->sun = (Color){ 255, 214, 160, 255 };
    light->ambient = (Color){ 42, 48, 68, 255 };
    light->enabled = true;
    light->orbit = true;
    light->revision = 1;
}

void Light_Update(LightEnv *light)
{
    if (!light->enabled || !light->orbit) return;

    double t = GetTime() * 0.32;
    Vector3 dir = Vector3Normalize((Vector3){ (float)cos(t), -0.62f, (float)sin(t) });
    float dot = dir.x * light->direction.x + dir.y * light->direction.y + dir.z * light->direction.z;
    if (dot > 1.0f) dot = 1.0f;
    if (dot < -1.0f) dot = -1.0f;
    light->direction = dir; /* sun / live tint every frame */
    /* Mesh color rebakes are expensive — only when the sun moves ~1°. */
    if (acosf(dot) > (1.0f * DEG2RAD))
        light->revision++;
}

Matrix Light_DrawMatrixEx(Model model, Vector3 position, Vector3 rotationAxis,
                          float rotationAngleDeg, Vector3 scale)
{
    /* Match DrawModelEx: scale -> rotate -> translate, then model.transform. */
    Matrix matScale = MatrixScale(scale.x, scale.y, scale.z);
    Matrix matRotation = MatrixRotate(rotationAxis, rotationAngleDeg * DEG2RAD);
    Matrix matTranslation = MatrixTranslate(position.x, position.y, position.z);
    Matrix matTransform = MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);
    return MatrixMultiply(model.transform, matTransform);
}

Matrix Light_DrawMatrix(Model model, Vector3 position, float scale)
{
    return Light_DrawMatrixEx(model, position, (Vector3){ 0.0f, 1.0f, 0.0f }, 0.0f,
                              (Vector3){ scale, scale, scale });
}

/*
 * Linear part of `m`:
 * - orthonormal (rigid) → M*n, no renormalize
 * - uniform scale (+ rotation) → (M/s)*n, no renormalize (mesh normals assumed unit)
 * - else → inverse-transpose, renormalize
 */
static void Light_NormalBasis(Matrix m, float out[9], int *needNormalize)
{
    const float eps = 1e-3f;
    /* Column lengths² and dots (raymath column-major). */
    float c0 = m.m0 * m.m0 + m.m1 * m.m1 + m.m2 * m.m2;
    float c1 = m.m4 * m.m4 + m.m5 * m.m5 + m.m6 * m.m6;
    float c2 = m.m8 * m.m8 + m.m9 * m.m9 + m.m10 * m.m10;
    float d01 = m.m0 * m.m4 + m.m1 * m.m5 + m.m2 * m.m6;
    float d02 = m.m0 * m.m8 + m.m1 * m.m9 + m.m2 * m.m10;
    float d12 = m.m4 * m.m8 + m.m5 * m.m9 + m.m6 * m.m10;

    int orthogonal = (fabsf(d01) < eps) && (fabsf(d02) < eps) && (fabsf(d12) < eps);
    int unitCols =
        (fabsf(c0 - 1.0f) < eps) && (fabsf(c1 - 1.0f) < eps) && (fabsf(c2 - 1.0f) < eps);

    if (orthogonal && unitCols)
    {
        out[0] = m.m0; out[1] = m.m1; out[2] = m.m2;
        out[3] = m.m4; out[4] = m.m5; out[5] = m.m6;
        out[6] = m.m8; out[7] = m.m9; out[8] = m.m10;
        *needNormalize = 0;
        return;
    }

    if (orthogonal && c0 > eps && fabsf(c0 - c1) < eps && fabsf(c1 - c2) < eps)
    {
        /* Uniform scale s: normal basis is R = M/s (directions only). */
        float invs = 1.0f / sqrtf(c0);
        out[0] = m.m0 * invs; out[1] = m.m1 * invs; out[2] = m.m2 * invs;
        out[3] = m.m4 * invs; out[4] = m.m5 * invs; out[5] = m.m6 * invs;
        out[6] = m.m8 * invs; out[7] = m.m9 * invs; out[8] = m.m10 * invs;
        *needNormalize = 0;
        return;
    }

    Matrix invT = MatrixTranspose(MatrixInvert(m));
    out[0] = invT.m0; out[1] = invT.m1; out[2] = invT.m2;
    out[3] = invT.m4; out[4] = invT.m5; out[5] = invT.m6;
    out[6] = invT.m8; out[7] = invT.m9; out[8] = invT.m10;
    *needNormalize = 1;
}

static Vector3 Light_Eval(Vector3 normal, Vector3 worldPos, const LightEnv *light, Vector3 pointPos, float pointIntensity)
{
    if (!light->enabled) return (Vector3){ 1.0f, 1.0f, 1.0f };

    Vector3 n = Vector3Normalize(normal);
    if (Vector3LengthSqr(n) < 0.0001f) n = (Vector3){ 0.0f, 1.0f, 0.0f };

    Vector3 toSun = Vector3Normalize(Vector3Negate(light->direction));
    float wrap = Vector3DotProduct(n, toSun) * 0.5f + 0.5f;
    if (wrap < 0.0f) wrap = 0.0f;

    Vector3 out = {
        (light->ambient.r + light->sun.r * wrap) / 255.0f,
        (light->ambient.g + light->sun.g * wrap) / 255.0f,
        (light->ambient.b + light->sun.b * wrap) / 255.0f
    };

    if (pointIntensity > 0.0f)
    {
        Vector3 toPoint = Vector3Subtract(pointPos, worldPos);
        float dist = Vector3Length(toPoint);
        if (dist > 0.0001f)
        {
            toPoint = Vector3Scale(toPoint, 1.0f / dist);
            float att = pointIntensity / (1.0f + dist * 0.38f);
            float spot = Vector3DotProduct(n, toPoint);
            if (spot < 0.0f) spot = 0.0f;
            float lamp = spot * att;
            out.x += 0.95f * lamp;
            out.y += 0.82f * lamp;
            out.z += 0.55f * lamp;
        }
    }

    return out;
}

Color Light_Tint(Color base, Vector3 normal, Vector3 worldPos, const LightEnv *light, Vector3 pointPos, float pointIntensity)
{
    Vector3 e = Light_Eval(normal, worldPos, light, pointPos, pointIntensity);
    return (Color){
        (unsigned char)Clamp255((float)base.r * e.x),
        (unsigned char)Clamp255((float)base.g * e.y),
        (unsigned char)Clamp255((float)base.b * e.z),
        base.a
    };
}

static void Light_EnsureColors(Mesh *mesh)
{
    if (mesh->colors != NULL) return;

    int bytes = mesh->vertexCount * 4;
    mesh->colors = (unsigned char *)MemAlloc((size_t)bytes);
    for (int i = 0; i < bytes; i += 4)
    {
        mesh->colors[i + 0] = 255;
        mesh->colors[i + 1] = 255;
        mesh->colors[i + 2] = 255;
        mesh->colors[i + 3] = 255;
    }
}

void Light_ApplyModel(Model model, Matrix transform, const LightEnv *light,
                      Vector3 pointPos, float pointIntensity)
{
    /* DrawMesh (rlsw) reads mesh.colors via COLOR_ARRAY — no VBO upload needed.
     * Per-mesh stamp: skip rewrite when light revision + point + transform match. */
    typedef struct {
        const Mesh *mesh;
        uint32_t rev;
        float pI;
        Vector3 pPos;
        float t0, t5, t10, t12, t13, t14; /* fingerprint linear + translation */
    } LightMeshStamp;
    static LightMeshStamp stamps[16];
    static int nStamps = 0;

    const int enabled = light->enabled;
    const float ambR = (float)light->ambient.r;
    const float ambG = (float)light->ambient.g;
    const float ambB = (float)light->ambient.b;
    const float sunR = (float)light->sun.r;
    const float sunG = (float)light->sun.g;
    const float sunB = (float)light->sun.b;
    const float sx = -light->direction.x;
    const float sy = -light->direction.y;
    const float sz = -light->direction.z;
    const int usePoint = enabled && (pointIntensity > 0.0f);
    const uint32_t rev = light->revision;

    float nmat[9];
    int needNormalize = 1;
    Light_NormalBasis(transform, nmat, &needNormalize);

    for (int m = 0; m < model.meshCount; m++)
    {
        Mesh *mesh = &model.meshes[m];
        if (mesh->vertices == NULL || mesh->vertexCount <= 0) continue;

        int stamp = -1;
        for (int s = 0; s < nStamps; s++)
        {
            if (stamps[s].mesh == mesh) { stamp = s; break; }
        }
        if (stamp >= 0
            && stamps[stamp].rev == rev
            && stamps[stamp].pI == pointIntensity
            && stamps[stamp].pPos.x == pointPos.x
            && stamps[stamp].pPos.y == pointPos.y
            && stamps[stamp].pPos.z == pointPos.z
            && stamps[stamp].t0 == transform.m0
            && stamps[stamp].t5 == transform.m5
            && stamps[stamp].t10 == transform.m10
            && stamps[stamp].t12 == transform.m12
            && stamps[stamp].t13 == transform.m13
            && stamps[stamp].t14 == transform.m14)
        {
            continue;
        }

        Light_EnsureColors(mesh);

        const float *verts = mesh->vertices;
        const float *norms = mesh->normals;
        unsigned char *cols = mesh->colors;
        const int n = mesh->vertexCount;

        if (!enabled)
        {
            for (int i = 0; i < n; i++)
            {
                cols[i * 4 + 0] = 255;
                cols[i * 4 + 1] = 255;
                cols[i * 4 + 2] = 255;
                cols[i * 4 + 3] = 255;
            }
            goto stamp_out;
        }

        for (int i = 0; i < n; i++)
        {
            float ox = 0.0f, oy = 1.0f, oz = 0.0f;
            if (norms != NULL)
            {
                ox = norms[i * 3 + 0];
                oy = norms[i * 3 + 1];
                oz = norms[i * 3 + 2];
            }

            /* World-space normal via normal basis. */
            float nx = nmat[0] * ox + nmat[3] * oy + nmat[6] * oz;
            float ny = nmat[1] * ox + nmat[4] * oy + nmat[7] * oz;
            float nz = nmat[2] * ox + nmat[5] * oy + nmat[8] * oz;

            if (needNormalize)
            {
                float n2 = nx * nx + ny * ny + nz * nz;
                if (n2 < 0.0001f) { nx = 0.0f; ny = 1.0f; nz = 0.0f; }
                else
                {
                    float inv = 1.0f / sqrtf(n2);
                    nx *= inv; ny *= inv; nz *= inv;
                }
            }
            else
            {
                float n2 = nx * nx + ny * ny + nz * nz;
                if (n2 < 0.0001f) { nx = 0.0f; ny = 1.0f; nz = 0.0f; }
            }

            float wrap = (nx * sx + ny * sy + nz * sz) * 0.5f + 0.5f;
            if (wrap < 0.0f) wrap = 0.0f;

            float er = (ambR + sunR * wrap) * (1.0f / 255.0f);
            float eg = (ambG + sunG * wrap) * (1.0f / 255.0f);
            float eb = (ambB + sunB * wrap) * (1.0f / 255.0f);

            if (usePoint)
            {
                float wx = transform.m0 * verts[i * 3 + 0] + transform.m4 * verts[i * 3 + 1]
                         + transform.m8 * verts[i * 3 + 2] + transform.m12;
                float wy = transform.m1 * verts[i * 3 + 0] + transform.m5 * verts[i * 3 + 1]
                         + transform.m9 * verts[i * 3 + 2] + transform.m13;
                float wz = transform.m2 * verts[i * 3 + 0] + transform.m6 * verts[i * 3 + 1]
                         + transform.m10 * verts[i * 3 + 2] + transform.m14;

                float px = pointPos.x - wx;
                float py = pointPos.y - wy;
                float pz = pointPos.z - wz;
                float dist = sqrtf(px * px + py * py + pz * pz);
                if (dist > 0.0001f)
                {
                    float inv = 1.0f / dist;
                    px *= inv; py *= inv; pz *= inv;
                    float att = pointIntensity / (1.0f + dist * 0.38f);
                    float spot = nx * px + ny * py + nz * pz;
                    if (spot < 0.0f) spot = 0.0f;
                    float lamp = spot * att;
                    er += 0.95f * lamp;
                    eg += 0.82f * lamp;
                    eb += 0.55f * lamp;
                }
            }

            cols[i * 4 + 0] = (unsigned char)Clamp255(255.0f * er);
            cols[i * 4 + 1] = (unsigned char)Clamp255(255.0f * eg);
            cols[i * 4 + 2] = (unsigned char)Clamp255(255.0f * eb);
            cols[i * 4 + 3] = 255;
        }

    stamp_out:
        if (stamp < 0)
        {
            if (nStamps < (int)(sizeof(stamps) / sizeof(stamps[0])))
                stamp = nStamps++;
            else
                stamp = (int)(rev % (uint32_t)(sizeof(stamps) / sizeof(stamps[0])));
            stamps[stamp].mesh = mesh;
        }
        stamps[stamp].rev = rev;
        stamps[stamp].pI = pointIntensity;
        stamps[stamp].pPos = pointPos;
        stamps[stamp].t0 = transform.m0;
        stamps[stamp].t5 = transform.m5;
        stamps[stamp].t10 = transform.m10;
        stamps[stamp].t12 = transform.m12;
        stamps[stamp].t13 = transform.m13;
        stamps[stamp].t14 = transform.m14;
    }
}

void Light_DrawSun(const LightEnv *light, Vector3 origin, float distance)
{
    if (!light->enabled) return;

    Vector3 sunPos = Vector3Add(origin, Vector3Scale(Vector3Negate(light->direction), distance));
    DrawSphere(sunPos, distance * 0.035f, (Color){ 255, 230, 140, 255 });
    DrawLine3D(origin, sunPos, (Color){ 255, 210, 120, 180 });
}
