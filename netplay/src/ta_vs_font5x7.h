/* 5x7 glyph shapes adapted from the user-provided ZAMN Xbox menu font.
   Plain bitmap alphabet; no runtime font file or texture required. */
static const uint8_t* font_rows(char c) {
  /* 5x7 uppercase font. Each byte uses low five bits, left to right. */
  static const uint8_t blank[7]={0,0,0,0,0,0,0};
#define G(name,a,b,c,d,e,f,g) static const uint8_t name[7]={a,b,c,d,e,f,g}
  G(A,14,17,17,31,17,17,17); G(B,30,17,17,30,17,17,30);
  G(C,14,17,16,16,16,17,14); G(D,30,17,17,17,17,17,30);
  G(E,31,16,16,30,16,16,31); G(F,31,16,16,30,16,16,16);
  G(G,14,17,16,23,17,17,15); G(H,17,17,17,31,17,17,17);
  G(I,31,4,4,4,4,4,31);      G(J,7,2,2,2,18,18,12);
  G(K,17,18,20,24,20,18,17); G(L,16,16,16,16,16,16,31);
  G(M,17,27,21,21,17,17,17); G(N,17,25,21,19,17,17,17);
  G(O,14,17,17,17,17,17,14); G(P,30,17,17,30,16,16,16);
  G(Q,14,17,17,17,21,18,13); G(R,30,17,17,30,20,18,17);
  G(S,15,16,16,14,1,1,30);   G(T,31,4,4,4,4,4,4);
  G(U,17,17,17,17,17,17,14); G(V,17,17,17,17,17,10,4);
  G(W,17,17,17,21,21,21,10); G(X,17,17,10,4,10,17,17);
  G(Y,17,17,10,4,4,4,4);     G(Z,31,1,2,4,8,16,31);
  G(N0,14,17,19,21,25,17,14); G(N1,4,12,4,4,4,4,14);
  G(N2,14,17,1,2,4,8,31);     G(N3,30,1,1,14,1,1,30);
  G(N4,2,6,10,18,31,2,2);     G(N5,31,16,16,30,1,1,30);
  G(N6,14,16,16,30,17,17,14); G(N7,31,1,2,4,8,8,8);
  G(N8,14,17,17,14,17,17,14); G(N9,14,17,17,15,1,1,14);
  G(DOT,0,0,0,0,0,12,12); G(COLON,0,12,12,0,12,12,0);
  G(DASH,0,0,0,31,0,0,0); G(GT,16,8,4,2,4,8,16);
  G(SLASH,1,2,2,4,8,8,16); G(LT,1,2,4,8,4,2,1);
  G(UNDERSCORE,0,0,0,0,0,0,31);
  G(PERCENT,25,26,2,4,8,11,19);
#undef G
  if (c>='a'&&c<='z') c=(char)(c-'a'+'A');
  switch(c){
    case 'A':return A;case 'B':return B;case 'C':return C;case 'D':return D;
    case 'E':return E;case 'F':return F;case 'G':return G;case 'H':return H;
    case 'I':return I;case 'J':return J;case 'K':return K;case 'L':return L;
    case 'M':return M;case 'N':return N;case 'O':return O;case 'P':return P;
    case 'Q':return Q;case 'R':return R;case 'S':return S;case 'T':return T;
    case 'U':return U;case 'V':return V;case 'W':return W;case 'X':return X;
    case 'Y':return Y;case 'Z':return Z;
    case '0':return N0;case '1':return N1;case '2':return N2;case '3':return N3;
    case '4':return N4;case '5':return N5;case '6':return N6;case '7':return N7;
    case '8':return N8;case '9':return N9;case '.':return DOT;case ':':return COLON;
    case '-':return DASH;case '>':return GT;case '<':return LT;case '/':return SLASH;
    case '%':return PERCENT;case '_':return UNDERSCORE;
    default:return blank;
  }
}

