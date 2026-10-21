#ifndef TA_VS_VIEW_H
#define TA_VS_VIEW_H
/* Presentation only: original full 512x224 canvas and the exact 6x12 playfield.
   Output pixels are doubled horizontally by the SNES renderer. The peer sends
   the same original 224x224 left-side picture as before; no wire change. */
typedef struct{float left,top,right,bottom;}TaVsView;
typedef struct{float left,top,right,bottom,u0,v0,u1,v1;}TaVsViewField;
static inline TaVsView ta_vs_view(unsigned width,unsigned height,unsigned widescreen){
 TaVsView v={0,0,0,0};if(!width||!height)return v;
 float displayAspect=widescreen?16.0f/9.0f:4.0f/3.0f;
 float pixelAspect=displayAspect*(float)height/(float)width;
 float h=(float)height*0.90f,w=h*(4.0f/3.0f)/pixelAspect;
 if(w>(float)width*0.90f){w=(float)width*0.90f;h=w*pixelAspect*(3.0f/4.0f);}
 v.left=((float)width-w)*0.5f;v.top=((float)height-h)*0.5f;v.right=v.left+w;v.bottom=v.top+h;return v;
}
static inline TaVsViewField ta_vs_view_peer(TaVsView v){
 float w=v.right-v.left,h=v.bottom-v.top;
 TaVsViewField p={v.left+w*(304.0f/512.0f),v.top+h*(24.0f/224.0f),v.left+w*(496.0f/512.0f),v.top+h*(216.0f/224.0f),16.0f,24.0f,208.0f,216.0f};return p;
}
#endif
