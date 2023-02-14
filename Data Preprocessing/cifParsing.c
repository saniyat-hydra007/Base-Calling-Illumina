/*  Code to read and write CIF version 1
 *  Integers in file are little endian, this is assumed in code (eg. intel architecture)
 *
 *  Format:
 *  "CIF" version(u1) datasize(u1) firstcycle(u2) #cycles(2) #clusters(4)
 *  Repeat: cycle, channel, cluster
 *
 * version: currently 1
 * datasize: number of bytes used for floats
 * firstcycle: offset for cycles
 * #cycles: number of cycles
 * #clusters: number of clusters
 * floats are signed and truncated to nearest integer, then rounded into range
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
//#include <err.h>
#include <errno.h>
//#include <glob.h>
#include <math.h>
#define _GNU_SOURCE
#include <stdarg.h>

#ifndef HAS_REALLOCF
void * reallocf(void * ptr, size_t t){
	void * ptr2;
	ptr2 = realloc(ptr,t);
	if(ptr2==NULL && ptr!=NULL){
	    free(ptr);
	}
	return ptr2;
}
#endif
#define NCHANNEL    4
typedef union { int8_t * i8; int16_t * i16; int32_t * i32;} encInt;
typedef enum { XFILE_UNKNOWN, XFILE_RAW, XFILE_GZIP, XFILE_BZIP2 } XFILE_MODE;
typedef union {  FILE * fh;} XFILE_TYPE;

struct _xfile_struct {
    XFILE_MODE mode;
    XFILE_TYPE ptr;
};

typedef struct _xfile_struct XFILE;

struct cifData {
    uint8_t version, datasize;
    uint16_t firstcycle, ncycle;
    uint32_t ncluster;
    encInt intensity;
};
typedef struct cifData * CIFDATA;


// Accessor functions
uint8_t cif_get_version ( const CIFDATA cif ){ return cif->version;}
uint8_t cif_get_datasize ( const CIFDATA cif ){ return cif->datasize;}
uint16_t cif_get_firstcycle ( const CIFDATA cif ){ return cif->firstcycle;}
uint16_t cif_get_ncycle ( const CIFDATA cif ){ return cif->ncycle;}
uint32_t cif_get_ncluster ( const CIFDATA cif ){ return cif->ncluster;}
encInt cif_get_const_intensities ( const CIFDATA cif ){ return cif->intensity; }

// Delete
void free_cif ( CIFDATA cif ){
	if ( NULL==cif ) return;
	if ( NULL!=cif->intensity.i8){ free(cif->intensity.i8); }
	free(cif);
}
XFILE _xstdout, _xstderr, _xstdin;
XFILE * xstdin = &_xstdin;
XFILE * xstdout = &_xstdout;
XFILE * xstderr = &_xstderr;
static bool _xinit = false;

void initialise_aybstd ( void ){
   _xstdin.mode = XFILE_RAW; _xstdin.ptr.fh = stdin;
   _xstdout.mode = XFILE_RAW; _xstdout.ptr.fh = stdout;
   _xstderr.mode = XFILE_RAW; _xstderr.ptr.fh = stderr;
   _xinit = true;
}



const char * find_suffix ( const char * fn ){
	const size_t len = strlen(fn);
size_t i;
	for (  i=len-1 ; i>0 ; i-- ){
		if ( fn[i] == '.') return fn+i+1;
	}
	if(fn[0]=='.') return fn+1;

	return fn+len; // Pointer to '\0' at end of string
}


XFILE_MODE guess_mode_from_filename ( const char * fn ){
	const char * suffix = find_suffix (fn);
	if ( strcmp(suffix,"gz") == 0 ){ return XFILE_GZIP;}
	if ( strcmp(suffix,"bz2") == 0 ){ return XFILE_BZIP2;}

	return XFILE_RAW;
}




int xfprintf( XFILE * fp, const char * fmt, ... ){
    int ret=EOF;
    va_list args;

    if(!_xinit){initialise_aybstd();}

    va_start(args,fmt);
    switch( fp->mode ){
      case XFILE_UNKNOWN:
      case XFILE_RAW:   ret=vfprintf(fp->ptr.fh,fmt,args); break;
    }
    va_end(args);
    return ret;
}

void xnull_file(XFILE * fp){
    switch( fp->mode ){
      case XFILE_UNKNOWN:
      case XFILE_RAW: fp->ptr.fh = NULL; break;
    }
}

int xnotnull_file(XFILE * fp){
    switch( fp->mode ){
      case XFILE_UNKNOWN:
      case XFILE_RAW:   if(NULL==fp->ptr.fh){ return 0;} break;
    }

    return 1;
}

void xfclose(XFILE * fp){
    if(!_xinit){initialise_aybstd();}
    if( NULL==fp || !xnotnull_file(fp) ){return;}

    switch( fp->mode ){
	  case XFILE_UNKNOWN:
      case XFILE_RAW:     fclose(fp->ptr.fh); break;
    }
    free(fp);
}

XFILE * xfopen(const char *  fn, const XFILE_MODE mode, const char * mode_str){
    if(!_xinit){initialise_aybstd();}
    XFILE * fp = malloc(sizeof(XFILE));
    int fail=0;

    fp->mode = mode;
    if ( XFILE_UNKNOWN==mode){ fp->mode = guess_mode_from_filename(fn);}

    switch ( fp->mode ){
      case XFILE_UNKNOWN:
      case XFILE_RAW:
            fp->ptr.fh = fopen(fn,mode_str);
            if(NULL==fp->ptr.fh){fail=1;}
            break;
      default: xnull_file(fp); fail=1;
    }

    if (fail){
        perror(fn);
        free(fp);
	return NULL;
    }

    return fp;
}


size_t xfread(void *ptr, size_t size, size_t nmemb, XFILE *fp){
    if(!_xinit){initialise_aybstd();}
    size_t ret = 0;

    switch( fp->mode ){
    	case XFILE_UNKNOWN:
        case XFILE_RAW:   ret = fread(ptr,size,nmemb,fp->ptr.fh); break;
    }

    return ret;
}

size_t xfwrite(const void * ptr, const size_t size, const size_t nmemb, XFILE * fp){
    if(!_xinit){initialise_aybstd();}
    size_t ret = 0;

    switch( fp->mode ){
    	case XFILE_UNKNOWN:
        case XFILE_RAW:   ret = fwrite(ptr,size,nmemb,fp->ptr.fh); break;
    }

    return ret;
}

int xfputc ( int c, XFILE * fp){
    if(!_xinit){initialise_aybstd();}
    int ret = EOF;
    switch(fp->mode){
    	case XFILE_UNKNOWN:
        case XFILE_RAW:   ret = fputc(c,fp->ptr.fh); break;
        }
    return ret;
}

int xfputs ( const char *  str, XFILE * fp){
    if(!_xinit){initialise_aybstd();}
    int ret = EOF;
    switch(fp->mode){
    	case XFILE_UNKNOWN:
        case XFILE_RAW:   ret = fputs(str,fp->ptr.fh); break;
    }
    return ret;
}


int xfgetc(XFILE * fp){
    char c=0;
    int ret = xfread(&c,sizeof(char),1,fp);
    return (ret!=0)?c:EOF;
}

char * xfgets( char * s, int n, XFILE * fp){
    int ret = xfread(s,sizeof(char),n-1,fp);
    s[ret] = '\0';
    return s;
}


char * xfgetln( XFILE * fp, size_t * len ){
    char * str = NULL;
    int size = 0;
    int c = 0;
    *len = 0;
    while ( c=xfgetc(fp), (c!=EOF) && (c!='\n') && (c!='\r') ){
    	if(size<=*len){
    	    size += 80;
            str = reallocf(str, size);
            if(NULL==str){ return NULL;}
        }
        str[*len] = c;
        (*len)++;
	}
	// Make sure there is sufficient memory for terminating '\0'
	if(size<=*len){
    	size += 1;
        str = reallocf(str, size);
        if(NULL==str){ return NULL;}
    }
    str[*len] = '\0';


	return str;
}



bool __attribute__((const)) isCifAllowedDatasize ( const uint8_t datasize );
encInt readCifIntensities ( XFILE * ayb_fp , const CIFDATA const header, encInt intensties );
encInt readEncodedFloats ( XFILE  * ayb_fp, const size_t nfloat, const uint8_t nbyte, encInt  tmp_mem );
bool writeCifHeader ( XFILE * ayb_fp, const CIFDATA const header);
bool writeCifIntensities ( XFILE * ayb_fp , const CIFDATA const header,
                           const encInt intensities );
bool writeEncodedFloats ( XFILE * ayb_fp , const size_t nfloat , const uint8_t nbyte,
                          const encInt floats );


bool __attribute__((const)) isCifAllowedDatasize ( const uint8_t datasize ){
    if ( 1==datasize || 2==datasize || 4==datasize ){ return true;}
    return false;
}



/* Read and check header */
CIFDATA readCifHeader (XFILE * ayb_fp){
    CIFDATA header = malloc(sizeof(struct cifData));
    if(NULL==header){return NULL;}
    char str[4] = "\0\0\0\0";
    const char * hd_str = "CIF";

    // Check if this is a CIF format file
    // "Magic number" is three bytes long
    xfread(&str,3,1,ayb_fp);
    // and should be the string "CIF"
    if ( strncmp(str,hd_str,3) ){
        free(header);
        return NULL;
    }
    // Rest of header.
    xfread(&header->version,   1,1,ayb_fp);
    xfread(&header->datasize,  1,1,ayb_fp);
    xfread(&header->firstcycle,2,1,ayb_fp);
    xfread(&header->ncycle,    2,1,ayb_fp);
    xfread(&header->ncluster,  4,1,ayb_fp);

    header->intensity.i32 = NULL;

    assert( 1==header->version);
    assert( isCifAllowedDatasize(header->datasize) );

    return header;
}

/* Read intensities */
encInt readCifIntensities ( XFILE * ayb_fp , const CIFDATA header, encInt  intensities ){
    const size_t size = NCHANNEL * header->ncluster * header->ncycle;
    intensities = readEncodedFloats(ayb_fp,size,header->datasize,intensities);
    return intensities;
}

/* Read an array of encoded floats and return array */
encInt readEncodedFloats ( XFILE  * ayb_fp, const size_t nfloat, const uint8_t nbyte, encInt intensities ){
    assert(1==nbyte || 2==nbyte || 4==nbyte);
    assert(nfloat>0);

    if ( NULL==intensities.i32 ){
        intensities.i8 = calloc(nfloat,nbyte);
    }
    xfread(intensities.i8,nbyte,nfloat,ayb_fp);

    return intensities;
}


/* Write header for CIF file */
bool writeCifHeader ( XFILE * ayb_fp, const CIFDATA const header){
    assert( isCifAllowedDatasize(header->datasize) );
    assert( 1==header->version );

    xfputs("CIF",ayb_fp);
    xfwrite(&header->version,   1,1,ayb_fp);
    xfwrite(&header->datasize,  1,1,ayb_fp);
    xfwrite(&header->firstcycle,2,1,ayb_fp);
    xfwrite(&header->ncycle,    2,1,ayb_fp);
    xfwrite(&header->ncluster,  4,1,ayb_fp);

    return true;
}

/*  Write intensities in encoded format
 *  Balance between speed and memory consumption could be changed by writing
 * out in more chunks.
 */
bool writeCifIntensities ( XFILE * ayb_fp , const CIFDATA  header,
                           const encInt  intensities ){
    const size_t nfloat = NCHANNEL * header->ncycle * header->ncluster;
    return writeEncodedFloats ( ayb_fp, nfloat, header->datasize, intensities );
}


int32_t __attribute__((const)) getMax (const uint8_t nbyte){
    switch(nbyte){
        case 1: return INT8_MAX;
        case 2: return INT16_MAX;
        case 4: return INT32_MAX;
        default:
            printf("Unimplemented bytesize %u in %s:%d\n",nbyte,__FILE__,__LINE__);
    }
    return 0.; // Never reach here
}

int32_t __attribute__((const)) getMin (const uint8_t nbyte){
    switch(nbyte){
        case 1: return INT8_MIN;
        case 2: return INT16_MIN;
        case 4: return INT32_MIN;
        default:
            printf("Unimplemented bytesize %u in %s:%d\n",nbyte,__FILE__,__LINE__);
    }
    return 0.; // Never reach here
}


/*  Write floats in encoded format
 */
bool writeEncodedFloats ( XFILE * ayb_fp , const size_t nfloat , const uint8_t nbyte,
                          const encInt  intensities ){
    xfwrite( intensities.i8, nbyte, nfloat, ayb_fp);
    return true;
}



/*  Routines to read and write CIF files  */

bool write2CIFstream ( XFILE * ayb_fp, const encInt  intensities, const uint16_t firstcycle, const uint32_t ncycle, const uint32_t ncluster, const uint8_t nbyte){
    assert(NULL!=ayb_fp);
    assert(NULL!=intensities.i8);
    assert( isCifAllowedDatasize(nbyte) );

    struct cifData header = { 1, nbyte, firstcycle, ncycle, ncluster, {.i8=NULL} };
    writeCifHeader(ayb_fp,&header);
    writeEncodedFloats(ayb_fp,ncluster*ncycle*NCHANNEL,nbyte,intensities);

    return true;
}


bool write2CIFfile ( const char * fn, const XFILE_MODE mode, const encInt  intensities, const uint16_t firstcycle, const uint32_t ncycle, const uint32_t ncluster, const uint8_t nbyte){
    if(NULL==fn){ return false;}
    XFILE * ayb_fp = xfopen(fn,mode,"wb");
    if ( NULL==ayb_fp){ return false;}
    bool ret = write2CIFstream(ayb_fp,intensities,firstcycle,ncycle,ncluster,nbyte);
    xfclose(ayb_fp);
    return ret;
}

bool writeCIFtoFile ( const CIFDATA const cif, const char * fn, const XFILE_MODE mode){
    if(NULL==cif){ return false;}
    if(NULL==fn){ return false;}
	return write2CIFfile(fn,mode,cif->intensity,cif->firstcycle,cif->ncycle,cif->ncluster,cif->datasize);
}

bool writeCIFtoStream ( const CIFDATA  cif, XFILE * ayb_fp){
    if(NULL==cif){ return false;}
    if(NULL==ayb_fp){ return false;}
    bool ret = write2CIFstream(ayb_fp,cif->intensity,cif->firstcycle,cif->ncycle,cif->ncluster,cif->datasize);
    return ret;
}


CIFDATA readCIFfromStream ( XFILE * ayb_fp ){
    CIFDATA cif = readCifHeader(ayb_fp);
    encInt e = {.i8=NULL};
    cif->intensity = readCifIntensities ( ayb_fp , cif, e );
    return cif;
}

CIFDATA readCIFfromFile ( const char * fn, const XFILE_MODE mode){
    XFILE * ayb_fp = xfopen(fn,mode,"rb");
    if ( NULL==ayb_fp){ return NULL;}
    CIFDATA cif = readCIFfromStream(ayb_fp);
    xfclose(ayb_fp);
    return cif;
}

bool consistent_cif_headers( const CIFDATA cif1, const CIFDATA cif2 ){
   if ( cif1->version  != cif2->version  ){ return false; } // Check might be relaxed later
   if ( cif1->datasize != cif2->datasize ){ return false; } // Check might be relaxed later
   if ( cif1->ncluster != cif2->ncluster ){ return false; }
   return true;
}

CIFDATA cif_add_file( const char * fn, const XFILE_MODE mode, CIFDATA cif ){
   CIFDATA newheader = NULL;
   XFILE * ayb_fp = NULL;
   if ( NULL==fn){ goto cif_add_error;}
   ayb_fp = xfopen(fn,mode,"rb");
   if ( NULL==ayb_fp){ goto cif_add_error;}

   newheader = readCifHeader(ayb_fp);
   if ( NULL==newheader ){ goto cif_add_error;}
   if ( NULL==cif->intensity.i8 ){
      cif->ncluster = newheader->ncluster;
      // First file read. Allocated memory needed
      cif->intensity.i8 = calloc(cif->ncycle*cif->ncluster*NCHANNEL,cif->datasize);
      if ( NULL==cif->intensity.i8 ){ goto cif_add_error;}
   }
   if ( ! consistent_cif_headers(cif,newheader) ){ goto cif_add_error;}
   const size_t offset = (newheader->firstcycle - 1) * cif->ncluster * NCHANNEL;
   encInt mem = {.i8=NULL};
   switch(cif->datasize){
       case 1: mem.i8 = cif->intensity.i8 + offset; break;
       case 2: mem.i16 = cif->intensity.i16 + offset; break;
       case 3: mem.i32 = cif->intensity.i32 + offset; break;
       default: printf("Incorrect datasize in %s (%s:%d)\n",__func__,__FILE__,__LINE__);
   }
   readCifIntensities(ayb_fp,newheader,mem);

   free_cif(newheader);
   return cif;

cif_add_error:
   free_cif(newheader);
   free_cif(cif);
   return NULL;
}


char * cif_create_cifglob ( const char * root, const uint32_t lane, const uint32_t tile ){
   if(NULL==root){ return NULL;}
   if(lane>9){
      printf("Assumption that lane numbering is less than 10 violated (asked for %u).\n",lane);
      return NULL;
   }
   if(tile>9999){
      printf("Assumption that tile numbering is less than 9999 violated (asked for %u).\n",tile);
      return NULL;
   }

   char * cif_glob = calloc(strlen(root)+41,sizeof(char));
   if(NULL==cif_glob){
      return NULL;
   }
   strcpy(cif_glob,root);
   uint32_t offset = strlen(cif_glob);
   strcpy(cif_glob+offset,"/Data/Intensities/L00");
   offset += 21;
   cif_glob[offset] = lane + 48;
   offset++;
   strcpy(cif_glob+offset,"/C*.1/s_X_");
   cif_glob[offset+8] = lane + 48;
   offset += 10;
   offset += sprintf(cif_glob+offset,"%u",tile);
   strcpy(cif_glob+offset,".cif");

   return cif_glob;
}

CIFDATA new_cif ( void ){
   CIFDATA cif = malloc(sizeof(*cif));
   if(NULL==cif){ return NULL; }
   cif->version = 1;
   cif->datasize = 2;
   cif->firstcycle = 1;
   cif->ncycle = 0;
   cif->ncluster = 0;
   cif->intensity.i8 = NULL;
   return cif;
}

CIFDATA spliceCIF(const CIFDATA cif, uint32_t ncycle, size_t offset){
    if(NULL==cif){return NULL;}
    if(offset+ncycle>cif->ncycle){ return NULL;}

    CIFDATA newcif = new_cif();
    memcpy(newcif,cif,sizeof(*newcif));
    newcif->firstcycle = 1;
    newcif->ncycle = ncycle;

    const size_t nobs = NCHANNEL*newcif->ncluster*newcif->ncycle;
    newcif->intensity.i8 = calloc(nobs,newcif->datasize);
    if(NULL==newcif->intensity.i8){ goto clean;}

    size_t offset8 = offset*NCHANNEL*newcif->ncluster*cif->datasize;
    memcpy(newcif->intensity.i8,cif->intensity.i8+offset8,nobs*newcif->datasize);

    return newcif;

clean:
    free(newcif);
    return NULL;
}



/*  Print routine for CIF structure */
void showCIF ( XFILE * ayb_fp, const CIFDATA const cif, uint32_t mcluster, uint32_t mcycle){
	static const char * basechar = "ACGT";
	if ( NULL==ayb_fp) return;
	if ( NULL==cif) return;

	xfprintf( ayb_fp, "@CIF Data version = %u\n", cif->version );
	xfprintf( ayb_fp, "@datasize = %u bytes\n", cif->datasize );
	xfprintf( ayb_fp, "@ncycles = %u\n", cif->ncycle);
	xfprintf( ayb_fp, "@first cycle = %u\n", cif->firstcycle);
	xfprintf( ayb_fp, "@nclusters = %u\n", cif->ncluster);

	if(0==mcluster){ mcluster = cif->ncluster; }
	if(0==mcycle){ mcycle = cif->ncycle; }
	mcluster = (cif->ncluster>mcluster)?mcluster:cif->ncluster;
	mcycle   = (cif->ncycle>mcycle)?mcycle:cif->ncycle;
int cluster, base, cycle;
	for (  cluster=0 ; cluster<mcluster ; cluster++){

		for (  base=0 ; base<4 ; base++){
            xfprintf( ayb_fp, "cluster_%d\t", cluster+1 );
			xfputc (basechar[base],ayb_fp);
			for (  cycle=0 ; cycle<mcycle ; cycle++){
			    float f;
			    switch(cif->datasize){
			        case 1: f = (float)cif->intensity.i8[(cycle*NCHANNEL+base)*cif->ncluster+cluster]; break;
			        case 2: f = (float)cif->intensity.i16[(cycle*NCHANNEL+base)*cif->ncluster+cluster]; break;
			        case 4: f = (float)cif->intensity.i32[(cycle*NCHANNEL+base)*cif->ncluster+cluster]; break;
			        default: f = NAN;
			    }
				xfprintf( ayb_fp, " %5.0f", f);
			}
			xfputc('\n',ayb_fp);
		}
	}
	if( mcluster!=cif->ncluster){
		xfprintf(ayb_fp,"%u clusters omitted. ", cif->ncluster - mcluster );
	}
	if( mcycle!=cif->ncycle ){
		xfprintf(ayb_fp,"%u cycles omitted. ", cif->ncycle - mcycle );
	}
	xfputc('\n',ayb_fp);
}


void WriteFrmtd(FILE *stream, char *format, ...) {
   va_list args;

   va_start(args, format);
   va_start(args, format);
   vfprintf(stream, format, args);
   va_end(args);
}


int main (){
struct cifData * CIFDATA;
const char* fn = "s_1_0001_end1.cif";
const char* fnNew = "s_1_0001_end1.txt";
CIFDATA = readCIFfromFile ( fn, XFILE_RAW);
uint8_t version = cif_get_version (CIFDATA);
uint8_t datasize = cif_get_datasize( CIFDATA );
uint16_t firstcycle = cif_get_firstcycle( CIFDATA );
uint16_t ncycle = cif_get_ncycle( CIFDATA );
uint32_t ncluster = cif_get_ncluster( CIFDATA );
printf("version %d\ndatasize %d\nfirstcycle %d\nncycle %d\nncluster %d\n", version,datasize,firstcycle,ncycle,ncluster);
XFILE * ayb_fp = xfopen(fnNew,XFILE_RAW,"w");
showCIF (ayb_fp, CIFDATA, CIFDATA->ncluster , CIFDATA->ncycle);
printf("\nwrite successful!!");
return 0;
}
