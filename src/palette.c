
static void hueToRgb(float *c, float p, float q, float t) {
    // Clip
    if (t < 0.0f)
        t += 1.0f;
    if (t > 1.0f)
        t -= 1.0f;
    // Compute
    if (t < 0.166666)
        *c = q + (p - q) * 6 * t;
    else if (t < 0.5)
        *c = p;
    else if (t < 0.666666)
        *c = (q + (p - q) * (.666666 - t) * 6);
    else
        *c = q;
}

void HSLToRGB(float H, float S, float L, float rgb[]) {
    float p, q;
    if (S == 0) {
        rgb[0] = rgb[1] = rgb[2] = L;
    } else {
        p = L < .50 ? L * (1 + S) : L + S - (L * S);
        q = 2 * L - p;
        hueToRgb(&rgb[0], p, q, H + .33333f);
        hueToRgb(&rgb[1], p, q, H);
        hueToRgb(&rgb[2], p, q, H - .33333f);
    }
}

