// Port by Kaerus, Anders Elo 2013

/*
 * Self Decrypting Archive
 * http://jgae.de/sda.htm
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int hexcase = 0;  /* hex output format. 0 - lowercase; 1 - uppercase        */
char *b64pad  = ""; /* base-64 pad character. "=" for strict RFC compliance   */
int chrsz   = 8;  /* bits per input character. 8 - ASCII; 16 - Unicode      */

/*int State31, Polynom31, State33, Polynom33,
    State64H, State64L, Polynom64, Butt;
*/
int Polynomials31[37] = {
    0x40c6e78f,0x44ea7b19,0x45da25ce,0x470c368e,0x4920f4c1,0x4a2fb865,
    0x4b641875,0x4d474412,0x4c175700,0x4e880047,0x50a5894c,0x51ae3883,
    0x531df126,0x563e62e8,0x586801c2,0x5bef4706,0x5c14c48a,0x5d06e2a7,
    0x5f2f8a72,0x623311d9,0x65616f52,0x668043b4,0x672161c9,0x67f0a6a8,
    0x6814750f,0x6c4920c3,0x6dca541b,0x6e97e1ed,0x70963ac8,0x72de5f24,
    0x7411688a,0x7502196b,0x76202331,0x7887a9e1,0x790621f4,0x7e79deae,
    0x7faca450
};


typedef struct {
    int *elem;
    size_t used;
    size_t size;
} Array;

typedef struct {
    char *str;
    size_t len;
    size_t size;
} Buffer;

void initArray(Array *a, size_t size) {
    a->elem = (int *)malloc(size * sizeof(int));
    a->used = 0;
    a->size = size;
}

void initBuffer(Buffer *b, size_t size) {
    b->str = (char *)malloc(size * sizeof(char));
    b->str[0] = 0;
    b->len = 0;
    b->size = size;
}

void insertArray(Array *a, int elem) {
    if (a->used == a->size) {
        a->size *= 2;
        a->elem = (int *)realloc(a->elem, a->size * sizeof(int));
    }
    a->elem[a->used++] = elem;
}

void pushBuffer(Buffer *b, char *str, int len){
    int l = (len ? len : strlen(str));

    if(b->len + l + 1 >= b->size) {
        b->size = len + b->size + 1;
        b->str = (char *)realloc(b->str, b->size);
    }
    memcpy(b->str + b->len, str, l);
    b->len += l;
    b->str[b->len + 1] = 0;
}

void freeArray(Array *a) {
    
    if(a->elem) free(a->elem);
    a->elem = 0;
    a->used = a->size = 0;
}

void freeBuffer(Buffer *b) {
    if(b->str) free(b->str);
    b->str = 0;
    b->len = b->size = 0;
}

typedef struct {
    int State31;
    int Polynom31;
    int State33;
    int Polynom33;
    int State64H;
    int State64L;
    int Polynom64;
    int Butt;
    Array *pnState;
    Array *Polynom;
    Buffer *buf;
} SDA;


/*
 * These functions implement the four basic operations the algorithm uses.
 */

int safe_add(int x, int y) {
    int lsw = (x & 0xFFFF) + (y & 0xFFFF);
    int msw = (x >> 16) + (y >> 16) + (lsw >> 16);
    return (msw << 16) | (lsw & 0xFFFF);
}

int bit_rol(int num, int cnt) {
    return (num << cnt) | (num >> (32 - cnt));
}

int md5_cmn(int q, int a, int b, int x, int s, int t) {
    return safe_add(bit_rol(safe_add(safe_add(a, q), safe_add(x, t)), s),b);
}
int md5_ff(int a, int b, int c, int d, int x, int s, int t) {
    return md5_cmn((b & c) | ((~b) & d), a, b, x, s, t);
}
int md5_gg(int a, int b, int c, int d, int x, int s, int t) {
    return md5_cmn((b & d) | (c & (~d)), a, b, x, s, t);
}
int md5_hh(int a, int b, int c, int d, int x, int s, int t) {
    return md5_cmn(b ^ c ^ d, a, b, x, s, t);
}
int md5_ii(int a, int b, int c, int d, int x, int s, int t) {
    return md5_cmn(c ^ (b | (~d)), a, b, x, s, t);
}

Array *xorArray(Array *a, Array *b){
    Array *c = (Array *)malloc(sizeof(Array));

    initArray(c, a->used);

    for(int i = 0; i < c->used; i++){
        c->elem[i] = a->elem[i] ^ b->elem[i];
    }

    return c;
}
/*
 * Calculate the MD5 of an array of little-endian words, and a bit length
 */
Array *core_md5(Array *ina, int len) {
    int *x = ina->elem;
    /* append padding */
    x[len >> 5] |= 0x80 << ((len) % 32);
    x[(((len + 64) >> 9) << 4) + 14] = len;
    
  
    int a =  1732584193;
    int b = -271733879;
    int c = -1732584194;
    int d =  271733878;

    for(int i = 0; i < ina->used; i += 16) {
        int olda = a;
        int oldb = b;
        int oldc = c;
        int oldd = d;
 
        a = md5_ff(a, b, c, d, x[i+ 0], 7 , -680876936);
        d = md5_ff(d, a, b, c, x[i+ 1], 12, -389564586);
        c = md5_ff(c, d, a, b, x[i+ 2], 17,  606105819);
        b = md5_ff(b, c, d, a, x[i+ 3], 22, -1044525330);
        a = md5_ff(a, b, c, d, x[i+ 4], 7 , -176418897);
        d = md5_ff(d, a, b, c, x[i+ 5], 12,  1200080426);
        c = md5_ff(c, d, a, b, x[i+ 6], 17, -1473231341);
        b = md5_ff(b, c, d, a, x[i+ 7], 22, -45705983);
        a = md5_ff(a, b, c, d, x[i+ 8], 7 ,  1770035416);
        d = md5_ff(d, a, b, c, x[i+ 9], 12, -1958414417);
        c = md5_ff(c, d, a, b, x[i+10], 17, -42063);
        b = md5_ff(b, c, d, a, x[i+11], 22, -1990404162);
        a = md5_ff(a, b, c, d, x[i+12], 7 ,  1804603682);
        d = md5_ff(d, a, b, c, x[i+13], 12, -40341101);
        c = md5_ff(c, d, a, b, x[i+14], 17, -1502002290);
        b = md5_ff(b, c, d, a, x[i+15], 22,  1236535329);

        a = md5_gg(a, b, c, d, x[i+ 1], 5 , -165796510);
        d = md5_gg(d, a, b, c, x[i+ 6], 9 , -1069501632);
        c = md5_gg(c, d, a, b, x[i+11], 14,  643717713);
        b = md5_gg(b, c, d, a, x[i+ 0], 20, -373897302);
        a = md5_gg(a, b, c, d, x[i+ 5], 5 , -701558691);
        d = md5_gg(d, a, b, c, x[i+10], 9 ,  38016083);
        c = md5_gg(c, d, a, b, x[i+15], 14, -660478335);
        b = md5_gg(b, c, d, a, x[i+ 4], 20, -405537848);
        a = md5_gg(a, b, c, d, x[i+ 9], 5 ,  568446438);
        d = md5_gg(d, a, b, c, x[i+14], 9 , -1019803690);
        c = md5_gg(c, d, a, b, x[i+ 3], 14, -187363961);
        b = md5_gg(b, c, d, a, x[i+ 8], 20,  1163531501);
        a = md5_gg(a, b, c, d, x[i+13], 5 , -1444681467);
        d = md5_gg(d, a, b, c, x[i+ 2], 9 , -51403784);
        c = md5_gg(c, d, a, b, x[i+ 7], 14,  1735328473);
        b = md5_gg(b, c, d, a, x[i+12], 20, -1926607734);

        a = md5_hh(a, b, c, d, x[i+ 5], 4 , -378558);
        d = md5_hh(d, a, b, c, x[i+ 8], 11, -2022574463);
        c = md5_hh(c, d, a, b, x[i+11], 16,  1839030562);
        b = md5_hh(b, c, d, a, x[i+14], 23, -35309556);
        a = md5_hh(a, b, c, d, x[i+ 1], 4 , -1530992060);
        d = md5_hh(d, a, b, c, x[i+ 4], 11,  1272893353);
        c = md5_hh(c, d, a, b, x[i+ 7], 16, -155497632);
        b = md5_hh(b, c, d, a, x[i+10], 23, -1094730640);
        a = md5_hh(a, b, c, d, x[i+13], 4 ,  681279174);
        d = md5_hh(d, a, b, c, x[i+ 0], 11, -358537222);
        c = md5_hh(c, d, a, b, x[i+ 3], 16, -722521979);
        b = md5_hh(b, c, d, a, x[i+ 6], 23,  76029189);
        a = md5_hh(a, b, c, d, x[i+ 9], 4 , -640364487);
        d = md5_hh(d, a, b, c, x[i+12], 11, -421815835);
        c = md5_hh(c, d, a, b, x[i+15], 16,  530742520);
        b = md5_hh(b, c, d, a, x[i+ 2], 23, -995338651);

        a = md5_ii(a, b, c, d, x[i+ 0], 6 , -198630844);
        d = md5_ii(d, a, b, c, x[i+ 7], 10,  1126891415);
        c = md5_ii(c, d, a, b, x[i+14], 15, -1416354905);
        b = md5_ii(b, c, d, a, x[i+ 5], 21, -57434055);
        a = md5_ii(a, b, c, d, x[i+12], 6 ,  1700485571);
        d = md5_ii(d, a, b, c, x[i+ 3], 10, -1894986606);
        c = md5_ii(c, d, a, b, x[i+10], 15, -1051523);
        b = md5_ii(b, c, d, a, x[i+ 1], 21, -2054922799);
        a = md5_ii(a, b, c, d, x[i+ 8], 6 ,  1873313359);
        d = md5_ii(d, a, b, c, x[i+15], 10, -30611744);
        c = md5_ii(c, d, a, b, x[i+ 6], 15, -1560198380);
        b = md5_ii(b, c, d, a, x[i+13], 21,  1309151649);
        a = md5_ii(a, b, c, d, x[i+ 4], 6 , -145523070);
        d = md5_ii(d, a, b, c, x[i+11], 10, -1120210379);
        c = md5_ii(c, d, a, b, x[i+ 2], 15,  718787259);
        b = md5_ii(b, c, d, a, x[i+ 9], 21, -343485551);

        a = safe_add(a, olda);
        b = safe_add(b, oldb);
        c = safe_add(c, oldc);
        d = safe_add(d, oldd);
    }

    Array *ret = (Array *)malloc(sizeof(Array));
    
    initArray(ret,4);
    
    insertArray(ret, a);
    insertArray(ret, b);
    insertArray(ret, c);
    insertArray(ret, d);

    return ret;
}

Array *str2binl(char *str) {
    Array *bin = (Array *)malloc(sizeof(Array));

    int len = strlen(str);
    
    initArray(bin,len*2);
    
    int mask = (1 << chrsz) - 1;
    
    for(int i = 0; i < len * chrsz; i += chrsz)
        bin->elem[i>>5] |= (str[i / chrsz] & mask) << (i%32);
    
    return bin;
}


int pn(SDA *sda) {
    int MSB;
    
    do {
        MSB = sda->State31 & 0x80000000;
        sda->State31 &= 0x7fffffff;

        if( sda->State31 & 1 )
            sda->State31 = ( sda->State31 >> 1 ) ^ sda->Polynom31;
        else
            sda->State31 >>= 1;

        if( sda->State33 & 0x80000000 ) sda->State31 |= 0x80000000;

        if( MSB )
            sda->State33 = ( sda->State33 << 1 ) ^ sda->Polynom33;
        else
            sda->State33 <<= 1;

        MSB = ( sda->State64H & 1 );
        sda->State64H >>= 1;
        sda->State64H |= sda->State64L & 0x80000000;

        if( MSB )
            sda->State64L = ( sda->State64L << 1 ) ^ sda->Polynom64;
        else
            sda->State64L <<= 1;
    } while( sda->State64L & sda->Butt );

    return (sda->State31 ^ sda->State33);
}


Array *compress8to7( char *str ) {
    Array *arr = (Array *)malloc(sizeof(Array));

    int len = strlen(str);
    
    initArray(arr,len*2);
    
    int s = 0;
    
    for(int i = 0; i < len; i += 8 ) {
        int val = str[i] << 1 & 0xfe;
        for(int j = 0; j < 7 && i+j < len + 1; j++ ) {
            int tmp = str[i+j+1] << ( j+2 );
            val |= tmp >> 8;
            insertArray(arr, val & 0xff );
            val = tmp & 0xff;
        }
    }
    
    return arr;
}

void expand7to8( SDA *sda, Array *array ) {
    char *p;

    int tmp, out;
    char chr;
    
    for(int i = 0; i < array->used; i += 7 ) {
        tmp = array->elem[ i ];
        out = tmp >> 1;
        chr = out & 0x7f;
        pushBuffer(sda->buf, &chr, 1);
        for(int j = 1; j < 8; j++ ) {
            out = ( tmp << ( 7-j ) ) & 0x7f;
            tmp = array->elem[ i+j ];
            chr = (out |= ( tmp & 0xff ) >> ( j+1 ) );
            pushBuffer(sda->buf,&chr,1);
        }
    }
}

Array *crypt( SDA *sda, Array *ina ) {
    Array *ota = (Array *)malloc(sizeof(Array));

    int count = ina->used;

    initArray(ota,count);
    
    for(int i = 0; i < count; i++ ) {
        ota->elem[ i ] = ina->elem[ i ] ^ pn(sda);
    }
    
    return ota;
}


char *b64_tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


int *b64_decode_tab() {
    static int decode[64] = {};
    
    for(int i = 0; i < 64; i++ ) {
        decode[ b64_tab[ i ] ] = i;
    }
    
    return decode;
}

int decode(int c){
    int i;
    
    for(i = 0; i < 64; i++){
        if(b64_tab[i] == c) break;
    }

    return i;
}

Array *b64_to_array( char *str ) {
    Array *arr = (Array *)malloc(sizeof(Array));
    int lng = strlen(str);
    int b1,b2,b3,b4,triplet;

    initArray(arr,lng);
    
    for(int i = 0; i < lng; i += 4 ) {
        b1 = str[i];
        b2 = ( i+1 < lng ) ? str[ i+1 ] : 0;
        b3 = ( i+2 < lng ) ? str[ i+2 ] : 0;
        b4 = ( i+3 < lng ) ? str[ i+3 ] : 0;
        triplet = ((decode( b1 ) << 18) & 0xffffff)
            | ((decode( b2 ) << 12) & 0x3ffff)
            | ((decode( b3 ) <<  6) & 0xfff)
            | ((decode( b4 )      ) & 0x3f);
        insertArray(arr, (triplet >> 16 ) & 0xff);
        if( b3 ) insertArray(arr, ( triplet >> 8 ) & 0xff);
        if( b4 ) insertArray(arr, triplet & 0xff);
    }

    return arr;
}

void array_to_form(SDA *sda, Array *arr) {
    int lng = arr->used;
    
    int b2,b3,triplet;

    Buffer *str = (Buffer *)malloc(sizeof(Buffer));

    initBuffer(str,lng);

    for(int i = 0; i < lng; i += 3 ) {
        b2 = ( i+1 < lng ) ? arr->elem[ i+1 ] : 0;
        b3 = ( i+2 < lng ) ? arr->elem[ i+2 ] : 0;
        triplet = ((arr->elem[i] << 16) & 0xffffff) 
            | ((   b2  <<  8) & 0xffff)
            | (    b3         & 0xff);

        pushBuffer(str,b64_tab + ((triplet >> 18) & 0x3f),1);
        pushBuffer(str,b64_tab + ((triplet >> 12) & 0x3f),1);

        if( b2 ) {
            pushBuffer(str,b64_tab + ((triplet >> 6) & 0x3f),1);
        }
        if( b3 ) {
            pushBuffer(str,b64_tab + (triplet & 0x3f),1);
        }
        if( i % 48 == 45 ) {
            pushBuffer(sda->buf,str->str,str->len);
            if( i < lng - 3 ) {
                str->len = 0;
                pushBuffer(str,"\n",1);
            }
        } else {
            if( i >= lng - 3 ) {
                pushBuffer(sda->buf,str->str,str->len);
            }
        }
    }

}

void initSDA( SDA *sda, char *passphr) {
    int plen = strlen(passphr);
    
    sda->buf = (Buffer *)malloc(sizeof(Buffer));
    initBuffer(sda->buf,256);

    Array *bin = str2binl( passphr );
    Array *pn = core_md5( bin, plen * chrsz );

    sda->pnState = pn;
    
    sda->State31 = sda->pnState->elem[ 0 ];

    if( !(sda->State31 & 0x7fffffff)) sda->State31++;

    sda->State33  = sda->pnState->elem[ 1 ];

    if( !sda->State33 ) sda->State33++;

    sda->State64H = sda->pnState->elem[ 2 ];
    sda->State64L = sda->pnState->elem[ 3 ];

    if( !sda->State64H && !sda->State64L ) sda->State64L++;

    Array *a = core_md5( sda->pnState, 0x80 );
    Array *b = core_md5( bin, plen * chrsz >> 1 );
    Array *x = xorArray(a,b);
    sda->Polynom = x;

    freeArray(a);
    freeArray(b);
    freeArray(bin);
    
    sda->Polynom31 = Polynomials31[ ( sda->Polynom->elem[ 0 ] >> 1 ) % 31 ];
    sda->Polynom33 = sda->Polynom->elem[ 1 ] | 1;
    sda->Polynom64 = sda->Polynom->elem[ 2 ] | 1;
    sda->Butt  = 1 << ( sda->Polynom->elem[ 3 ] & 0x1f );
    sda->Butt |= 1 << ( ( sda->Polynom->elem[ 3 ] >> 8 ) & 0x1f );
}

void freeSDA( SDA *sda ) {
    if(sda->buf) {
        freeBuffer(sda->buf);
        free(sda->buf);
        sda->buf = 0;
    }
    if(sda->pnState) {
        freeArray(sda->pnState);
        sda->pnState = 0;
    }
    if(sda->Polynom) {
        freeArray(sda->Polynom);
        sda->Polynom = 0;
    }
}


SDA *encrypt(char *input, char *passphrase) {
    SDA *sda = (SDA *) malloc(sizeof(SDA));

    initSDA(sda, passphrase );

    Array *a = compress8to7( input );
    Array *b = crypt( sda, a );
    array_to_form(sda, b );

    freeArray(a);
    freeArray(b);
    
    return sda;
}

SDA *decrypt(char *input, char *passphrase) {
    SDA *sda = (SDA *) malloc(sizeof(SDA));

    initSDA(sda, passphrase);

    Array *a = b64_to_array( input );
    Array *b = crypt( sda, a);

    expand7to8(sda, b);

    freeArray(a);
    freeArray(b);
    
    return sda;
}

int main(){
    SDA *sda = encrypt("hello world", "secret");

    printf("encrypted %s\n", sda->buf->str);

    freeSDA(sda);
}
