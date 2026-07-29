#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>

int main(int argc, char **argv) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

    C2D_Font fontReg = C2D_FontLoad("sdmc:/3ds/poeticware/spaceReg.bcfnt");
    C2D_Font fontItal = C2D_FontLoad("sdmc:/3ds/poeticware/spaceItal.bcfnt");

    C2D_TextBuf textBuf = C2D_TextBufNew(1024);

    C2D_Text textReg;
    C2D_TextFontParse(&textReg, fontReg, textBuf, "regular words look like this");
    C2D_TextOptimize(&textReg);

    C2D_Text textItal;
    C2D_TextFontParse(&textItal, fontItal, textBuf, "italic words look like this");
    C2D_TextOptimize(&textItal);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(top);

        C2D_DrawText(&textReg, C2D_WithColor, 40.0f, 80.0f, 0.5f, 1.0f, 1.0f, C2D_Color32(255, 255, 255, 255));
        C2D_DrawText(&textItal, C2D_WithColor, 40.0f, 120.0f, 0.5f, 1.0f, 1.0f, C2D_Color32(255, 255, 255, 255));

        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(textBuf);
    C2D_FontFree(fontReg);
    C2D_FontFree(fontItal);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}