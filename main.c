// ============================================================
//  MEMORY MATCHING GAME  v3
//  Fixes: pause, grids 2x2/4x4/6x6, infinite Colors+Shapes,
//         star rating on WIN, game-over no stars
// ============================================================

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX

#include <windows.h>
#include "raylib.h"
#include "raymath.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

// --- Constants -----------------------------------------------
#define SCREEN_W       900
#define SCREEN_H       700
#define MAX_GRID       6
#define MAX_CARDS      36
#define MAX_PARTICLES  200
#define MAX_STARS      80
#define HIGHSCORE_FILE "highscore.dat"
#define HS_MAGIC       0xA7B3C9D2

// ============================================================
//  GRID SUPPORT: 2x2, 4x4, 6x6
//  Fruits/Animals: 8 image files -> max 8 pairs -> safe for 2x2(2p), 4x4(8p)
//  Colors/Shapes: procedurally generated, 18 unique entries -> covers 6x6(18p)
//
//  MOVE LIMIT:  base = pairs*3,  Beginner*2.0  Moderate*1.4  Expert*1.0
//
//  STAR RATING (WIN only):
//    3 stars : moves <= pairs * 1.5   (brilliant / near-perfect)
//    2 stars : moves <= moveLimit*0.6 (solid play)
//    1 star  : any win
//    0 stars : GAMEOVER screen only   (loss shown as empty stars)
// ============================================================

typedef enum { MENU, SETTINGS, HIGHSCORE_SCREEN, GAME, PAUSE, WIN, GAMEOVER } GameState;
typedef enum { FRUITS, ANIMALS, COLORS, SHAPES } Theme;
typedef enum { BEGINNER, MODERATE, EXPERT } Difficulty;

typedef struct {
    int       value;
    bool      revealed, matched;
    float     flipT;
    bool      hovered;
    float     glowAlpha;
    Rectangle rect;
} Card;

typedef struct {
    Vector2 pos, vel;
    float   life, maxLife;
    Color   color;
    float   size;
} Particle;

typedef struct {
    Vector2 pos;
    float   speed, alpha, radius;
} Star;

typedef struct {
    int   score[3];
    int   moves[3];
    float time[3];
} GridHS;

typedef struct {
    unsigned int magic;
    GridHS       gs[3];   // 0=2x2  1=4x4  2=6x6
} HSData;

// --- Globals -------------------------------------------------
Card      cards[MAX_GRID][MAX_GRID];
Particle  particles[MAX_PARTICLES];
Star      stars[MAX_STARS];
int       pCount = 0;

Texture2D fruitTex[8];
Texture2D animalTex[8];
Texture2D cardBack;

GameState  state   = MENU;
Theme      theme   = FRUITS;
Difficulty diff    = MODERATE;
int        grid    = 4;

int   firstR=-1, firstC=-1, secondR=-1, secondC=-1;
int   matches=0, waitFrames=0;
float timer=0.f;
int   score=0, moves=0, moveLimit=0;
int   starRating=0;
bool  isNewRecord=false;
float bgAnim=0.f, titleWave=0.f;
float fadeAlpha=1.f;
bool  fadingIn=true;

HSData hsData;

// 18-color palette covers 6x6 (18 pairs) for Colors theme
Color colPalette[18] = {
    {220,60,60,255},   {60,120,220,255},  {60,190,80,255},
    {240,200,40,255},  {160,60,220,255},  {240,130,40,255},
    {40,200,200,255},  {200,40,140,255},  {120,200,60,255},
    {200,120,40,255},  {80,80,220,255},   {220,220,60,255},
    {40,160,160,255},  {180,40,80,255},   {100,220,160,255},
    {200,100,200,255}, {60,180,220,255},  {220,160,60,255},
};

// shape: 0=circle 1=square 2=triangle 3=ring 4=star 5=diamond
static void GetShapeDesc(int pairId, int *shapeOut, int *colorOut) {
    *shapeOut = pairId % 6;
    *colorOut = pairId % 18;
}

// --- Forward declarations ------------------------------------
void  LoadHS(void); void SaveHS(void);
bool  TryUpdateHS(int g, Difficulty d, int sc, int mv, float tm);
void  InitGame(void);
void  UpdateGame(void);
void  DrawGame(void);
void  DrawMenu(void);
void  DrawSettings(void);
void  DrawHighScoreScreen(void);
void  DrawWin(void);
void  DrawGameOver(void);
void  DrawPause(void);
void  DrawHUD(void);
void  SpawnBurst(float x, float y, Color c);
void  UpdateParticles(void); void DrawParticles(void);
void  DrawBackground(void);
void  DrawCard(int r, int c);
bool  Button(Rectangle r, const char *txt, Color base, int fontSize);
void  DrawFancyTitle(const char *txt, int x, int y, int size, float t);
void  InitStars(void); void UpdateStars(void); void DrawStars(void);
void  DrawStarRating(int numStars, int cx, int cy);
Color ThemeAccent(void);
int   GridIndex(int g);
int   ComputeMoveLimit(int g, Difficulty d);
int   ComputeScore(int g, Difficulty d, int mv, float t);
int   ComputeStars(int mv, int pairs, int lim);

// ============================================================
//  HELPERS
// ============================================================
int GridIndex(int g) {
    if (g==2) return 0; if (g==4) return 1; if (g==6) return 2; return -1;
}
int ComputeMoveLimit(int g, Difficulty d) {
    int pairs=(g*g)/2, base=pairs*3;
    float m=(d==BEGINNER)?2.0f:(d==MODERATE)?1.4f:1.0f;
    return (int)(base*m);
}
int ComputeScore(int g, Difficulty d, int mv, float t) {
    int   pairs=(g*g)/2, lim=ComputeMoveLimit(g,d);
    float dm=(d==BEGINNER)?1.0f:(d==MODERATE)?1.5f:2.5f;
    int base=pairs*100;
    int mb=(lim-mv)<0?0:(lim-mv)*15;
    int tb=(int)(300.f-t)*2; if(tb<0)tb=0;
    int nb=(mv<=(int)(pairs*1.5f+0.5f))?500:0;
    return (int)((base+mb+tb+nb)*dm);
}
int ComputeStars(int mv, int pairs, int lim) {
    if (mv<=(int)(pairs*1.5f+0.5f)) return 3;
    if (mv<=(int)(lim*0.6f))        return 2;
    return 1;
}
Color ThemeAccent(void) {
    switch(theme){
        case FRUITS:  return (Color){80,220,100,255};
        case ANIMALS: return (Color){220,160,60,255};
        case COLORS:  return (Color){100,180,255,255};
        case SHAPES:  return (Color){220,100,180,255};
    }
    return WHITE;
}

// ============================================================
//  HIGH SCORE
// ============================================================
void LoadHS(void) {
    memset(&hsData,0,sizeof(hsData));
    for(int g=0;g<3;g++) for(int d=0;d<3;d++){
        hsData.gs[g].moves[d]=9999; hsData.gs[g].time[d]=9999.f;
    }
    FILE *f=fopen(HIGHSCORE_FILE,"rb");
    if(!f) return;
    HSData tmp;
    if(fread(&tmp,sizeof(tmp),1,f)==1 && tmp.magic==HS_MAGIC) hsData=tmp;
    fclose(f);
}
void SaveHS(void) {
    hsData.magic=HS_MAGIC;
    FILE *f=fopen(HIGHSCORE_FILE,"wb");
    if(f){fwrite(&hsData,sizeof(hsData),1,f);fclose(f);}
}
bool TryUpdateHS(int g, Difficulty d, int sc, int mv, float tm) {
    int gi=GridIndex(g); if(gi<0) return false;
    int di=(int)d;
    if(sc>hsData.gs[gi].score[di]){
        hsData.gs[gi].score[di]=sc;
        hsData.gs[gi].moves[di]=mv;
        hsData.gs[gi].time[di] =tm;
        SaveHS(); return true;
    }
    return false;
}

// ============================================================
//  PARTICLES
// ============================================================
void SpawnBurst(float x, float y, Color c) {
    for(int i=0;i<14&&pCount<MAX_PARTICLES;i++){
        float a=(float)(rand()%360)*DEG2RAD, s=1.5f+(rand()%30)*0.1f;
        particles[pCount++]=(Particle){{x,y},{cosf(a)*s,sinf(a)*s-1.5f},1.f,1.f,c,3.f+(rand()%4)};
    }
}
void UpdateParticles(void) {
    for(int i=0;i<pCount;i++){
        particles[i].pos.x+=particles[i].vel.x;
        particles[i].pos.y+=particles[i].vel.y;
        particles[i].vel.y+=0.08f;
        particles[i].life-=0.018f;
        if(particles[i].life<=0){particles[i]=particles[--pCount];i--;}
    }
}
void DrawParticles(void) {
    for(int i=0;i<pCount;i++){
        float a=particles[i].life/particles[i].maxLife;
        Color c=particles[i].color; c.a=(unsigned char)(a*255);
        DrawCircleV(particles[i].pos,particles[i].size*a,c);
    }
}

// ============================================================
//  BACKGROUND STARS
// ============================================================
void InitStars(void) {
    for(int i=0;i<MAX_STARS;i++)
        stars[i]=(Star){{(float)(rand()%SCREEN_W),(float)(rand()%SCREEN_H)},
                         0.3f+(rand()%20)*0.05f,(float)(rand()%200)/255.f+0.1f,1.f+(rand()%3)};
}
void UpdateStars(void) {
    for(int i=0;i<MAX_STARS;i++){stars[i].pos.y+=stars[i].speed;if(stars[i].pos.y>SCREEN_H)stars[i].pos.y=0;}
}
void DrawStars(void) {
    for(int i=0;i<MAX_STARS;i++){Color c=WHITE;c.a=(unsigned char)(stars[i].alpha*180);DrawCircleV(stars[i].pos,stars[i].radius,c);}
}

// ============================================================
//  DRAW STAR RATING
//  Layout: [small] [LARGE] [small]  — center star is biggest
//  cx,cy = horizontal center, top of the star group
// ============================================================
void DrawStarRating(int numStars, int cx, int cy) {
    // x offsets and sizes for the 3 stars
    float offX[3] = { -62.f, 0.f, 62.f };
    float offY[3] = {  12.f, 0.f, 12.f };   // side stars slightly lower
    float sz[3]   = {  26.f, 38.f, 26.f };  // center star larger

    for(int s=0;s<3;s++){
        float sx = (float)cx + offX[s];
        float sy = (float)cy + offY[s];
        float r  = sz[s];
        bool  lit = (s < numStars);

        Color fill = lit ? (Color){255,215,30,255} : (Color){55,55,65,220};
        Color rim  = lit ? (Color){255,160,0,255}  : (Color){90,90,100,180};

        // 5-point star via fan triangles
        for(int p=0;p<5;p++){
            float a1=(p*72.f-90.f)*DEG2RAD;
            float a2=((p*72.f+36.f)-90.f)*DEG2RAD;
            float a3=((p*72.f+72.f)-90.f)*DEG2RAD;
            Vector2 ctr={sx,sy};
            Vector2 o1={sx+cosf(a1)*r,       sy+sinf(a1)*r};
            Vector2 in1={sx+cosf(a2)*r*0.42f, sy+sinf(a2)*r*0.42f};
            Vector2 o2={sx+cosf(a3)*r,       sy+sinf(a3)*r};
            DrawTriangle(o1,in1,ctr,fill);
            DrawTriangle(in1,o2,ctr,fill);
        }
        // Rim lines
        for(int p=0;p<5;p++){
            float a1=(p*72.f-90.f)*DEG2RAD;
            float a2=((p*72.f+36.f)-90.f)*DEG2RAD;
            float a3=((p*72.f+72.f)-90.f)*DEG2RAD;
            Vector2 o1={sx+cosf(a1)*r,       sy+sinf(a1)*r};
            Vector2 in1={sx+cosf(a2)*r*0.42f, sy+sinf(a2)*r*0.42f};
            Vector2 o2={sx+cosf(a3)*r,       sy+sinf(a3)*r};
            DrawLineV(o1,in1,rim); DrawLineV(in1,o2,rim);
        }
        // Shine on lit stars
        if(lit)
            DrawCircle((int)(sx-r*0.22f),(int)(sy-r*0.28f),(int)(r*0.13f),(Color){255,255,255,110});
    }
}

// ============================================================
//  BACKGROUND
// ============================================================
void DrawBackground(void) {
    bgAnim+=GetFrameTime()*0.4f;
    Color top,bot;
    switch(theme){
        case FRUITS:  top=(Color){30,90,40,255}; bot=(Color){10,50,20,255}; break;
        case ANIMALS: top=(Color){40,70,30,255}; bot=(Color){20,40,15,255}; break;
        case COLORS:  top=(Color){20,30,70,255}; bot=(Color){10,15,45,255}; break;
        case SHAPES:  top=(Color){50,20,60,255}; bot=(Color){25,10,40,255}; break;
        default:      top=(Color){20,20,40,255}; bot=(Color){10,10,20,255}; break;
    }
    DrawRectangleGradientV(0,0,SCREEN_W,SCREEN_H,top,bot);
    Color acc=ThemeAccent();
    for(int i=0;i<6;i++){
        float ox=sinf(bgAnim+i*1.3f)*300+SCREEN_W/2;
        float oy=cosf(bgAnim*0.7f+i*0.9f)*200+SCREEN_H/2;
        DrawCircle((int)ox,(int)oy,60+i*10,(Color){acc.r,acc.g,acc.b,12});
    }
    DrawStars();
}

// ============================================================
//  FANCY TITLE
// ============================================================
void DrawFancyTitle(const char *txt, int x, int y, int size, float t) {
    int len=(int)strlen(txt), cx=x;
    for(int i=0;i<len;i++){
        char ch[2]={txt[i],0};
        float wy=sinf(t*2.f+i*0.5f)*8.f;
        DrawText(ch,cx+3,y+3+(int)wy,size,(Color){0,0,0,80});
        Color c=(i%3==0)?ThemeAccent():(i%3==1)?WHITE:(Color){200,240,255,255};
        DrawText(ch,cx,y+(int)wy,size,c);
        cx+=MeasureText(ch,size)+2;
    }
}

// ============================================================
//  BUTTON
// ============================================================
bool Button(Rectangle r, const char *txt, Color base, int fontSize) {
    Vector2 m=GetMousePosition();
    bool hover=CheckCollisionPointRec(m,r);
    float sc=hover?1.06f:1.f;
    Rectangle sr={r.x-r.width*(sc-1)/2,r.y-r.height*(sc-1)/2,r.width*sc,r.height*sc};
    DrawRectangleRounded((Rectangle){sr.x+3,sr.y+4,sr.width,sr.height},0.35f,10,(Color){0,0,0,80});
    Color c=hover?(Color){(unsigned char)fminf(base.r+40,255),(unsigned char)fminf(base.g+40,255),(unsigned char)fminf(base.b+40,255),255}:base;
    DrawRectangleRounded(sr,0.35f,10,c);
    DrawRectangleRounded((Rectangle){sr.x+4,sr.y+4,sr.width-8,sr.height*0.4f},0.35f,8,(Color){255,255,255,30});
    int tw=MeasureText(txt,fontSize);
    int tx=(int)(sr.x+sr.width/2-tw/2), ty=(int)(sr.y+sr.height/2-fontSize/2);
    DrawText(txt,tx+1,ty+1,fontSize,(Color){0,0,0,60});
    DrawText(txt,tx,ty,fontSize,WHITE);
    return hover&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// ============================================================
//  INIT GAME
// ============================================================
void InitGame(void) {
    if(grid<2)grid=2; if(grid>6)grid=6; if(grid%2!=0)grid++;

    int total=grid*grid, pairs=total/2;
    int arr[MAX_CARDS];
    for(int i=0;i<pairs;i++){arr[i*2]=i;arr[i*2+1]=i;}
    // Fisher-Yates shuffle
    for(int i=total-1;i>0;i--){int j=rand()%(i+1),t=arr[i];arr[i]=arr[j];arr[j]=t;}

    int cellSize=480/grid;
    int padX=(SCREEN_W-cellSize*grid)/2;
    int padY=(SCREEN_H-cellSize*grid)/2+10;
    int gap=6;

    for(int i=0;i<grid;i++) for(int j=0;j<grid;j++){
        int k=i*grid+j;
        cards[i][j]=(Card){arr[k],false,false,1.f,false,0.f,
            {(float)(padX+j*cellSize+gap/2),(float)(padY+i*cellSize+gap/2),
             (float)(cellSize-gap),(float)(cellSize-gap)}};
    }

    moveLimit=ComputeMoveLimit(grid,diff);
    matches=moves=score=0; timer=0.f;
    firstR=firstC=secondR=secondC=-1;
    waitFrames=0; pCount=0;
    starRating=0; isNewRecord=false;
    fadingIn=true; fadeAlpha=1.f;
}

// ============================================================
//  DRAW CARD CONTENT (front face)
// ============================================================
static void DrawCardContent(int pairId, Rectangle inner) {
    switch(theme){
        case FRUITS:{
            int idx=pairId%8;
            if(fruitTex[idx].id)
                DrawTexturePro(fruitTex[idx],(Rectangle){0,0,(float)fruitTex[idx].width,(float)fruitTex[idx].height},inner,(Vector2){0,0},0,WHITE);
            else{
                DrawRectangleRounded(inner,0.2f,8,colPalette[idx%18]);
                DrawText(TextFormat("F%d",idx),(int)(inner.x+inner.width/2-10),(int)(inner.y+inner.height/2-10),20,WHITE);
            }
            break;
        }
        case ANIMALS:{
            int idx=pairId%8;
            if(animalTex[idx].id)
                DrawTexturePro(animalTex[idx],(Rectangle){0,0,(float)animalTex[idx].width,(float)animalTex[idx].height},inner,(Vector2){0,0},0,WHITE);
            else{
                DrawRectangleRounded(inner,0.2f,8,colPalette[idx%18]);
                DrawText(TextFormat("A%d",idx),(int)(inner.x+inner.width/2-10),(int)(inner.y+inner.height/2-10),20,WHITE);
            }
            break;
        }
        case COLORS:{
            Color col=colPalette[pairId%18];
            DrawRectangleRounded(inner,0.2f,8,col);
            DrawRectangleRounded((Rectangle){inner.x+4,inner.y+4,inner.width-8,inner.height*0.38f},0.3f,6,(Color){255,255,255,55});
            break;
        }
        case SHAPES:{
            int shapeId,colorId; GetShapeDesc(pairId,&shapeId,&colorId);
            Color sc=colPalette[colorId];
            float cx=inner.x+inner.width/2, cy=inner.y+inner.height/2, sz=inner.width*0.36f;
            switch(shapeId){
                case 0: // circle
                    DrawCircle((int)cx,(int)cy,(int)sz,sc);
                    DrawCircle((int)(cx-sz*0.25f),(int)(cy-sz*0.3f),(int)(sz*0.18f),(Color){255,255,255,80});
                    break;
                case 1: // square
                    DrawRectangleRounded((Rectangle){cx-sz,cy-sz,sz*2,sz*2},0.15f,6,sc);
                    DrawRectangleRounded((Rectangle){cx-sz+4,cy-sz+4,sz*2-8,(sz*2)*0.35f},0.15f,4,(Color){255,255,255,50});
                    break;
                case 2: // triangle
                    DrawTriangle((Vector2){cx,cy-sz},(Vector2){cx-sz,cy+sz},(Vector2){cx+sz,cy+sz},sc);
                    break;
                case 3: // ring
                    DrawRing((Vector2){cx,cy},sz*0.45f,sz,0,360,32,sc);
                    break;
                case 4: // star
                    for(int p=0;p<5;p++){
                        float a1=(p*72.f-90.f)*DEG2RAD, a2=((p*72.f+36.f)-90.f)*DEG2RAD, a3=((p*72.f+72.f)-90.f)*DEG2RAD;
                        Vector2 ctr={cx,cy},o1={cx+cosf(a1)*sz,cy+sinf(a1)*sz},
                                in2={cx+cosf(a2)*sz*0.42f,cy+sinf(a2)*sz*0.42f},
                                o2={cx+cosf(a3)*sz,cy+sinf(a3)*sz};
                        DrawTriangle(o1,in2,ctr,sc); DrawTriangle(in2,o2,ctr,sc);
                    }
                    break;
                case 5: // diamond
                    DrawTriangle((Vector2){cx,cy-sz},(Vector2){cx-sz*0.7f,cy},(Vector2){cx+sz*0.7f,cy},sc);
                    DrawTriangle((Vector2){cx-sz*0.7f,cy},(Vector2){cx+sz*0.7f,cy},(Vector2){cx,cy+sz},sc);
                    break;
            }
            break;
        }
    }
}

// ============================================================
//  DRAW CARD
// ============================================================
void DrawCard(int r, int c) {
    Card *cd=&cards[r][c];
    Rectangle rect=cd->rect;
    if(cd->hovered&&!cd->revealed&&!cd->matched) rect.y-=4;

    float ft=cd->flipT;
    bool showFront=(ft<0.5f)||cd->matched;
    float squish=fabsf(ft-0.5f)*2.f;
    float drawW=rect.width*squish;
    float drawX=rect.x+(rect.width-drawW)/2.f;
    Rectangle drawR={drawX,rect.y,drawW,rect.height};
    if(drawW<2) return;

    if(!showFront){
        if(cardBack.id)
            DrawTexturePro(cardBack,(Rectangle){0,0,(float)cardBack.width,(float)cardBack.height},drawR,(Vector2){0,0},0,WHITE);
        else{
            DrawRectangleRounded(drawR,0.15f,8,(Color){40,60,140,255});
            for(int pi=0;pi<4;pi++){
                float px=drawR.x+6+pi*(drawR.width/4);
                DrawRectangleRounded((Rectangle){px,drawR.y+6,drawR.width/4-4,drawR.height-12},0.2f,4,(Color){60,90,180,80});
            }
        }
    } else {
        Color bg=cd->matched?(Color){40,160,80,255}:(Color){240,240,230,255};
        DrawRectangleRounded(drawR,0.15f,8,bg);
        if(cd->matched){
            cd->glowAlpha+=GetFrameTime()*3.f;
            float ga=(sinf(cd->glowAlpha)*0.5f+0.5f)*60.f;
            DrawRectangleRounded(drawR,0.15f,8,(Color){100,255,140,(unsigned char)ga});
        }
        Rectangle inner={drawR.x+4,drawR.y+4,drawR.width-8,drawR.height-8};
        DrawCardContent(cd->value,inner);
    }

    if(cd->hovered&&!cd->revealed&&!cd->matched) DrawRectangleRoundedLines(drawR,0.15f,8,ThemeAccent());
    if(cd->matched) DrawRectangleRoundedLines(drawR,0.15f,8,(Color){180,255,180,200});
}

// ============================================================
//  UPDATE GAME
// ============================================================
void UpdateGame(void) {
    float dt=GetFrameTime();
    timer+=dt; titleWave+=dt;

    // P or Escape -> pause (KEY_NULL set so Escape doesn't close window)
    if(IsKeyPressed(KEY_P)||IsKeyPressed(KEY_ESCAPE)){state=PAUSE;return;}

    Vector2 mouse=GetMousePosition();

    for(int i=0;i<grid;i++) for(int j=0;j<grid;j++){
        Card *cd=&cards[i][j];

        // Flip animation
        if(cd->revealed||cd->matched){cd->flipT-=dt*7.f;if(cd->flipT<0.f)cd->flipT=0.f;}
        else{cd->flipT+=dt*7.f;if(cd->flipT>1.f)cd->flipT=1.f;}

        cd->hovered=CheckCollisionPointRec(mouse,cd->rect);

        if(waitFrames==0&&cd->hovered&&IsMouseButtonPressed(MOUSE_LEFT_BUTTON)&&!cd->revealed&&!cd->matched){
            if(firstR==i&&firstC==j) continue;
            cd->revealed=true;
            if(firstR==-1){firstR=i;firstC=j;}
            else{secondR=i;secondC=j;moves++;waitFrames=(diff==EXPERT?28:diff==MODERATE?55:90);}
        }
    }

    if(waitFrames>0){
        waitFrames--;
        if(waitFrames==0){
            Card *a=&cards[firstR][firstC],*b=&cards[secondR][secondC];
            if(a->value==b->value){
                a->matched=b->matched=true; matches++;
                Color burst=colPalette[a->value%18];
                SpawnBurst(a->rect.x+a->rect.width/2,a->rect.y+a->rect.height/2,burst);
                SpawnBurst(b->rect.x+b->rect.width/2,b->rect.y+b->rect.height/2,burst);
            } else {a->revealed=b->revealed=false;}
            firstR=firstC=secondR=secondC=-1;
        }
    }

    UpdateParticles(); UpdateStars();

    // WIN
    if(matches==(grid*grid)/2){
        int pairs=(grid*grid)/2;
        score=ComputeScore(grid,diff,moves,timer);
        starRating=ComputeStars(moves,pairs,moveLimit);
        isNewRecord=TryUpdateHS(grid,diff,score,moves,timer);
        fadingIn=false; fadeAlpha=0.f; state=WIN; return;
    }
    // GAME OVER
    if(moves>=moveLimit){fadingIn=false;fadeAlpha=0.f;state=GAMEOVER;}
}

// ============================================================
//  HUD
// ============================================================
void DrawHUD(void) {
    DrawRectangle(0,0,SCREEN_W,54,(Color){0,0,0,140});
    int mins=(int)(timer/60),secs=(int)(timer)%60;
    DrawText(TextFormat("Time %02d:%02d",mins,secs),14,16,18,WHITE);

    int remaining=moveLimit-moves;
    Color mc=WHITE;
    if(remaining<=moveLimit/4)mc=(Color){255,80,80,255};
    else if(remaining<=moveLimit/2)mc=(Color){255,200,60,255};

    DrawText(TextFormat("Moves: %d",moves),170,16,18,WHITE);
    DrawText(TextFormat("Left: %d",remaining),310,16,18,mc);

    float bw=160.f,fw=bw*((float)remaining/moveLimit);
    if(fw<0)fw=0;
    DrawRectangleRounded((Rectangle){310,38,bw,8},0.5f,4,(Color){60,60,60,180});
    DrawRectangleRounded((Rectangle){310,38,fw,8},0.5f,4,mc);

    DrawText(TextFormat("Pairs: %d/%d",matches,(grid*grid)/2),490,16,18,WHITE);

    const char *dn[3]={"Beginner","Moderate","Expert"};
    DrawText(TextFormat("%s | %dx%d",dn[diff],grid,grid),640,16,16,ThemeAccent());

    int gi=GridIndex(grid);
    if(gi>=0){
        int best=hsData.gs[gi].score[(int)diff];
        DrawText(TextFormat("Best: %d",best),640,36,14,(Color){255,230,80,200});
    }
    DrawText("[P]=Pause",806,20,13,(Color){180,180,180,120});
}

// ============================================================
//  DRAW GAME
// ============================================================
void DrawGame(void) {
    DrawBackground(); DrawHUD();
    for(int i=0;i<grid;i++) for(int j=0;j<grid;j++) DrawCard(i,j);
    DrawParticles();
}

// ============================================================
//  MENU
// ============================================================
void DrawMenu(void) {
    DrawBackground(); titleWave+=GetFrameTime();
    DrawFancyTitle("MEMORY MATCH",140,70,56,titleWave);

    int gi=GridIndex(grid);
    int best=(gi>=0)?hsData.gs[gi].score[(int)diff]:0;
    DrawRectangleRounded((Rectangle){295,152,310,34},0.4f,8,(Color){0,0,0,100});
    const char *dn[3]={"Beginner","Moderate","Expert"};
    DrawText(TextFormat("Best [%dx%d %s]: %d",grid,grid,dn[diff],best),305,160,15,(Color){255,230,80,255});

    if(Button((Rectangle){340,210,220,50},"PLAY",       (Color){50,100,200,255},22)){InitGame();state=GAME;fadingIn=true;fadeAlpha=1.f;}
    if(Button((Rectangle){340,278,220,50},"SETTINGS",   (Color){60,140,80,255}, 22)) state=SETTINGS;
    if(Button((Rectangle){340,346,220,50},"HIGH SCORE", (Color){140,100,40,255},22)) state=HIGHSCORE_SCREEN;
    if(Button((Rectangle){340,414,220,50},"EXIT",       (Color){180,50,50,255}, 22)) CloseWindow();

    DrawText("Click cards to find matching pairs!",245,648,17,(Color){200,200,200,140});
}

// ============================================================
//  SETTINGS
// ============================================================
void DrawSettings(void) {
    DrawBackground();
    DrawFancyTitle("SETTINGS",270,28,46,titleWave);
    Color selBg=(Color){60,150,220,255},unsel=(Color){50,50,80,255};

    // Grid
    DrawText("GRID SIZE",60,104,20,ThemeAccent());
    if(Button((Rectangle){60, 132,145,44},"2 x 2",grid==2?selBg:unsel,17)) grid=2;
    if(Button((Rectangle){225,132,145,44},"4 x 4",grid==4?selBg:unsel,17)) grid=4;
    if(Button((Rectangle){390,132,145,44},"6 x 6",grid==6?selBg:unsel,17)) grid=6;

    // Difficulty
    DrawText("DIFFICULTY",60,198,20,ThemeAccent());
    if(Button((Rectangle){60, 226,168,44},"Beginner",diff==BEGINNER?selBg:(Color){40,130,60,255}, 16)) diff=BEGINNER;
    if(Button((Rectangle){248,226,168,44},"Moderate",diff==MODERATE?selBg:(Color){160,100,20,255},16)) diff=MODERATE;
    if(Button((Rectangle){436,226,168,44},"Expert",  diff==EXPERT  ?selBg:(Color){160,30,30,255}, 16)) diff=EXPERT;

    DrawRectangleRounded((Rectangle){60,284,720,76},0.15f,8,(Color){0,0,0,100});
    const char *desc[3]={
        "Beginner  |  Long flip delay  |  2.0x move limit  |  1.0x score multiplier",
        "Moderate  |  Normal flip      |  1.4x move limit  |  1.5x score multiplier",
        "Expert    |  Fast flip        |  1.0x move limit  |  2.5x score multiplier"};
    DrawText(desc[diff],78,299,15,(Color){220,220,200,255});

    int lim=ComputeMoveLimit(grid,diff);
    DrawText(TextFormat("Move limit for %dx%d %s: %d moves",grid,grid,
             diff==BEGINNER?"Beginner":diff==MODERATE?"Moderate":"Expert",lim),
             78,346,15,(Color){255,200,80,220});

    // Theme
    DrawText("THEME",60,395,20,ThemeAccent());
    if(Button((Rectangle){60, 422,150,44},"Fruits", theme==FRUITS ?selBg:(Color){40,130,60,255},  16)) theme=FRUITS;
    if(Button((Rectangle){226,422,150,44},"Animals",theme==ANIMALS?selBg:(Color){100,70,30,255},  16)) theme=ANIMALS;
    if(Button((Rectangle){392,422,150,44},"Colors", theme==COLORS ?selBg:(Color){40,80,180,255},  16)) theme=COLORS;
    if(Button((Rectangle){558,422,150,44},"Shapes", theme==SHAPES ?selBg:(Color){120,40,140,255}, 16)) theme=SHAPES;

    if(theme==FRUITS||theme==ANIMALS)
        DrawText("Fruits/Animals use your image files (8 unique). Best with 2x2 or 4x4.",60,480,14,(Color){200,200,160,200});
    else
        DrawText("Colors & Shapes are generated — 18 unique pairs, works perfectly with 6x6!",60,480,14,(Color){180,220,180,200});

    if(Button((Rectangle){340,526,220,46},"BACK",(Color){60,60,100,255},20)) state=MENU;
}

// ============================================================
//  HIGH SCORE SCREEN
// ============================================================
void DrawHighScoreScreen(void) {
    DrawBackground();
    DrawFancyTitle("HIGH SCORES",190,28,48,titleWave);
    const char *dn[3]={"Beginner","Moderate","Expert"};
    const char *gn[3]={"2 x 2","4 x 4","6 x 6"};
    Color dc[3]={{80,200,100,255},{240,180,40,255},{255,80,80,255}};

    for(int gi=0;gi<3;gi++){
        int by=105+gi*130;
        DrawRectangleRounded((Rectangle){28,(float)by,92,118},0.2f,6,(Color){0,0,0,110});
        DrawText(gn[gi],33,by+48,18,ThemeAccent());
        for(int di=0;di<3;di++){
            int cx=132+di*250;
            int sc=hsData.gs[gi].score[di],mv=hsData.gs[gi].moves[di];
            float tm=hsData.gs[gi].time[di];
            DrawRectangleRounded((Rectangle){(float)cx,(float)by,238,118},0.2f,8,(Color){0,0,0,110});
            DrawText(dn[di],cx+76,by+7,17,dc[di]);
            if(sc>0){
                DrawText(TextFormat("%d pts",sc),cx+60,by+31,21,WHITE);
                DrawText(TextFormat("Moves: %d",mv),cx+12,by+60,15,(Color){200,230,200,220});
                int ts=(int)tm;
                DrawText(TextFormat("Time: %02d:%02d",ts/60,ts%60),cx+12,by+82,15,(Color){200,210,255,220});
            } else {
                DrawText("---",cx+94,by+47,22,(Color){100,100,100,180});
            }
        }
    }
    if(Button((Rectangle){340,506,220,46},"BACK",(Color){60,60,100,255},20)) state=MENU;
}

// ============================================================
//  PAUSE  — rendered on top of live game
// ============================================================
void DrawPause(void) {
    // Dim overlay
    DrawRectangle(0,0,SCREEN_W,SCREEN_H,(Color){0,0,0,165});

    // Panel
    float px=272,py=168,pw=356,ph=348;
    DrawRectangleRounded((Rectangle){px+4,py+5,pw,ph},0.15f,10,(Color){0,0,0,120});   // shadow
    DrawRectangleRounded((Rectangle){px,py,pw,ph},0.15f,10,(Color){14,14,44,248});
    DrawRectangleRoundedLines((Rectangle){px,py,pw,ph},0.15f,10,ThemeAccent());

    DrawFancyTitle("PAUSED",(int)(px+38),(int)(py+18),44,titleWave);

    if(Button((Rectangle){(int)(px+48),298,260,50},"RESUME",    (Color){50,160,80,255}, 20)) state=GAME;
    if(Button((Rectangle){(int)(px+48),364,260,50},"RESTART",   (Color){60,100,200,255},20)){InitGame();state=GAME;}
    if(Button((Rectangle){(int)(px+48),430,260,50},"MAIN MENU", (Color){60,60,100,255}, 20)) state=MENU;
}

// ============================================================
//  WIN SCREEN  — star rating displayed
// ============================================================
void DrawWin(void) {
    DrawBackground(); titleWave+=GetFrameTime();
    if((int)(titleWave*10)%6==0){float bx=(float)(rand()%SCREEN_W);SpawnBurst(bx,-10,colPalette[rand()%18]);}
    DrawParticles(); UpdateParticles(); UpdateStars();

    DrawRectangleRounded((Rectangle){155,88,590,498},0.12f,10,(Color){0,0,0,185});

    DrawFancyTitle("YOU  WIN!",190,105,50,titleWave);

    // Star rating centered
    DrawStarRating(starRating, SCREEN_W/2, 182);

    // Performance label
    const char *perf[4]={"","Good Job!","Great!","Brilliant!"};
    Color       pcol[4]={WHITE,{180,230,180,255},{255,210,60,255},{100,255,150,255}};
    if(starRating>0)
        DrawText(perf[starRating],(int)(SCREEN_W/2-MeasureText(perf[starRating],20)/2),228,20,pcol[starRating]);

    int mins=(int)(timer/60),secs=(int)(timer)%60,pairs=(grid*grid)/2;
    const char *dn[3]={"Beginner","Moderate","Expert"};

    DrawText(TextFormat("Score:  %d pts",score),              218,258,24,(Color){255,230,60,255});
    DrawText(TextFormat("Time:   %02d:%02d",mins,secs),       218,292,20,WHITE);
    DrawText(TextFormat("Moves:  %d / %d limit",moves,moveLimit),218,320,20,WHITE);
    DrawText(TextFormat("Min possible: ~%d moves",pairs),     218,346,17,(Color){180,220,180,200});
    DrawText(TextFormat("Grid: %dx%d | %s",grid,grid,dn[diff]),218,370,17,ThemeAccent());

    if(starRating==3)
        DrawText("BRILLIANT SOLVE! +500 bonus!",210,396,17,(Color){100,255,150,255});
    if(isNewRecord)
        DrawText("NEW HIGH SCORE!",270,420,20,(Color){255,220,40,255});

    if(Button((Rectangle){182,450,192,46},"PLAY AGAIN",(Color){50,160,80,255}, 16)){InitGame();state=GAME;}
    if(Button((Rectangle){390,450,192,46},"MENU",      (Color){60,60,140,255},16)) state=MENU;
    if(Button((Rectangle){286,508,252,42},"HIGH SCORE",(Color){140,100,40,255},16)) state=HIGHSCORE_SCREEN;
}

// ============================================================
//  GAME OVER  — 0 empty stars shown (no rating earned)
// ============================================================
void DrawGameOver(void) {
    DrawBackground(); titleWave+=GetFrameTime(); UpdateStars();

    DrawRectangleRounded((Rectangle){175,105,550,450},0.12f,10,(Color){75,0,0,215});
    DrawRectangleRoundedLines((Rectangle){175,105,550,450},0.12f,10,(Color){200,60,60,180});

    DrawFancyTitle("GAME  OVER",182,124,48,titleWave);

    // 0 empty stars — makes it clear no rating was earned
    DrawStarRating(0, SCREEN_W/2, 205);
    DrawText("No stars — puzzle unsolved",
             (int)(SCREEN_W/2-MeasureText("No stars — puzzle unsolved",16)/2),
             248,16,(Color){140,140,140,200});

    int pairs=(grid*grid)/2;
    const char *dn[3]={"Beginner","Moderate","Expert"};
    DrawText(TextFormat("Move limit: %d reached!",moveLimit),  222,284,20,(Color){255,120,80,255});
    DrawText(TextFormat("Pairs found: %d / %d",matches,pairs), 250,316,19,WHITE);
    DrawText(TextFormat("Grid: %dx%d | %s",grid,grid,dn[diff]),250,346,18,ThemeAccent());
    DrawText("Tip: try Beginner or a smaller grid",222,378,16,(Color){200,200,170,200});

    if(Button((Rectangle){194,418,208,46},"TRY AGAIN",(Color){50,160,80,255}, 17)){InitGame();state=GAME;}
    if(Button((Rectangle){430,418,192,46},"MENU",     (Color){60,60,140,255},17)) state=MENU;
    if(Button((Rectangle){298,474,276,42},"SETTINGS", (Color){60,100,60,255}, 17)) state=SETTINGS;
}

// ============================================================
//  MAIN
// ============================================================
int main(void) {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL,exePath,MAX_PATH);
    char *ls=strrchr(exePath,'\\');
    if(ls){*ls='\0';SetCurrentDirectoryA(exePath);}

    InitWindow(SCREEN_W,SCREEN_H,"Memory Match v3");
    SetTargetFPS(60);
    srand((unsigned)time(NULL));
    SetExitKey(KEY_NULL);  // Escape handled manually (goes to pause)

    fruitTex[0]=LoadTexture("assets/apple.png");
    fruitTex[1]=LoadTexture("assets/banana.png");
    fruitTex[2]=LoadTexture("assets/mango.png");
    fruitTex[3]=LoadTexture("assets/orange.png");
    fruitTex[4]=LoadTexture("assets/grapes.png");
    fruitTex[5]=LoadTexture("assets/litchi.png");
    fruitTex[6]=LoadTexture("assets/guava.png");
    fruitTex[7]=LoadTexture("assets/watermelon.png");

    animalTex[0]=LoadTexture("assets/lion.png");
    animalTex[1]=LoadTexture("assets/dog.png");
    animalTex[2]=LoadTexture("assets/cat.png");
    animalTex[3]=LoadTexture("assets/elephant.png");
    animalTex[4]=LoadTexture("assets/penguin.png");
    animalTex[5]=LoadTexture("assets/rabbit.png");
    animalTex[6]=LoadTexture("assets/bird.png");
    animalTex[7]=LoadTexture("assets/tortoise.png");

    cardBack=LoadTexture("assets/card_back.png");

    for(int i=0;i<8;i++){
        if(fruitTex[i].id) {GenTextureMipmaps(&fruitTex[i]); SetTextureFilter(fruitTex[i],TEXTURE_FILTER_BILINEAR);}
        if(animalTex[i].id){GenTextureMipmaps(&animalTex[i]);SetTextureFilter(animalTex[i],TEXTURE_FILTER_BILINEAR);}
    }

    LoadHS();
    InitStars();

    while(!WindowShouldClose()){
        titleWave+=GetFrameTime();
        BeginDrawing();
        ClearBackground(BLACK);

        switch(state){
            case MENU:             DrawMenu();                break;
            case SETTINGS:         DrawSettings();            break;
            case HIGHSCORE_SCREEN: DrawHighScoreScreen();     break;
            case GAME:             UpdateGame(); DrawGame();  break;
            case PAUSE:            DrawGame(); DrawPause();   break;  // game visible behind
            case WIN:              DrawWin();                 break;
            case GAMEOVER:         DrawGameOver();            break;
        }

        if(fadingIn){
            fadeAlpha-=GetFrameTime()*2.f;
            if(fadeAlpha<0.f){fadeAlpha=0.f;fadingIn=false;}
            DrawRectangle(0,0,SCREEN_W,SCREEN_H,(Color){0,0,0,(unsigned char)(fadeAlpha*255)});
        }

        EndDrawing();
    }

    for(int i=0;i<8;i++){
        if(fruitTex[i].id)  UnloadTexture(fruitTex[i]);
        if(animalTex[i].id) UnloadTexture(animalTex[i]);
    }
    UnloadTexture(cardBack);
    CloseWindow();
    return 0;
}
