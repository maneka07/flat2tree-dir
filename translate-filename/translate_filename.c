#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <linux/limits.h> 
#include <string.h>
#include <openssl/sha.h>
#include <limits.h>
#include <assert.h>
#include <dirent.h>

#define DIR_CACHE_SIZE 1

#define VERBOSE_LEVEL 0     /*Set to 1 to get debugging messages printed*/
#define VERBOSE_LEVEL_ERROR 1

#define MAX_FILENAME_LENGTH 255

#define PRINT_MSG(verbose, ...) \
	if(verbose) printf(__VA_ARGS__)	\

struct cur_dir{
	DIR    *dirp;
	long   loc;
	FILE   *metafile;
	struct dirent 	fake_dirent;
	struct dirent64 fake_dirent64;
};

struct cur_dir _cur_dir = {NULL, -1}; 
static int  _nest_depth = 0;
static char _dir_cache[DIR_CACHE_SIZE][PATH_MAX];
static int  _last_cached = -1;

/*If dir is special, return file handle to opened metadata file*/
static int is_special_dir(const char *dir, int open_meta, FILE **metadata)
{
	int i;
	int found = 0;
	char metafname[PATH_MAX];
	
	/*Check if dir is special, i.e. it has been re-organized from flat
	 * to tree structure. 
	 * Currently, the library can detect only one reorganized directory 
	 * - which is specified via environment variable
	 * */
	 
	 if(_last_cached == -1){
		 /*Init cache*/
		for(i = 0; i < DIR_CACHE_SIZE; i++)
			_dir_cache[i][0] = '\0';
			
		_last_cached = 0;	
		
		char *s = getenv("FLAT_TO_NESTED_DIR");
		if(s == NULL){
			PRINT_MSG(VERBOSE_LEVEL_ERROR, "Warning: Please set FLAT_TO_NESTED_DIR environment variable!\n");
			return 0;
		} else {
			if(s[0] == '~'){
				char* home = getenv("HOME");
				strcpy(_dir_cache[_last_cached], home);
				strcat(_dir_cache[_last_cached], "/");
				strcat(_dir_cache[_last_cached], &s[1]);
			} else 
				strcpy(_dir_cache[_last_cached], s); 
			
			if(_dir_cache[_last_cached][strlen(_dir_cache[_last_cached])-1] == '/')
				_dir_cache[_last_cached][strlen(_dir_cache[_last_cached])-1] = '\0';
				
			PRINT_MSG(VERBOSE_LEVEL, "FLAT_TO_NESTED_DIR set to %s\n", _dir_cache[_last_cached]);
			_last_cached++;
		}
	}
	 	
	 if(_last_cached > 0) {
		char tmpdir[PATH_MAX];
		strcpy(tmpdir, dir);
		
		if(dir[strlen(dir)-1]=='/')
			tmpdir[strlen(dir)-1]='\0';
		for(i = 0; i < DIR_CACHE_SIZE; i++){
			PRINT_MSG(VERBOSE_LEVEL, "compare %s and %s\n", tmpdir, _dir_cache[i]); 
			if(strcmp(_dir_cache[i], tmpdir) == 0){
				found = 1;
				PRINT_MSG(VERBOSE_LEVEL, "%s is special dir!\n", tmpdir);
				break;
			}
		}
	 }

	if(found && open_meta){
		strcpy(metafname, dir);
		strcat(metafname, "/.flat_metadata");
		*metadata = fopen(metafname, "rb");
		
		if(*metadata == NULL){ 
			PRINT_MSG(VERBOSE_LEVEL_ERROR, "Warning: didn't find .flat_metadata file in the directory %s\n", dir);
			return 0;
		}
	} 
	
	return found;
}

/*return 0 - file directory wasn't reorganized, hence no need to translate
 *       1 - file directory was reorganized, hence return real_file path*/
static int translated_fname(/*in*/const char *file, /*out*/char* real_file)
{
	char *last_slsh;
	int i, slice; 
	int open_flag = 0;
	const char *fname;
	unsigned char hash[SHA_DIGEST_LENGTH];
	unsigned char hexsubdir[SHA_DIGEST_LENGTH];
	FILE *metafile;
	
	last_slsh = strrchr(file, '/');
	if(last_slsh == NULL) //no path in the filename
		return 0;
	if(strstr(file, ".flat_metadata") != NULL)
		return 0;
		
	memcpy(real_file, file, last_slsh - file);
	real_file[last_slsh - file]='\0';
	if(_nest_depth == 0) open_flag = 1;
	if(!is_special_dir(real_file, open_flag, &metafile))
		return 0;
		
	PRINT_MSG(VERBOSE_LEVEL, "Opening file in special directory\n");
	strcat(real_file, "/");
	assert(_nest_depth != 0);
	
	slice = (unsigned int)(SHA_DIGEST_LENGTH *2 / _nest_depth);
	//PRINT_MSG(VERBOSE_LEVEL,"slice %d\n", slice);	
	//real_file[last_slsh - file + 1]='\0'; 
	//PRINT_MSG(VERBOSE_LEVEL,"dir %s, ", real_file);
	fname = &file[last_slsh - file+1];
	PRINT_MSG(VERBOSE_LEVEL,"dir %s, fname %s\n", real_file, fname);
	//Hash it!
	SHA1((const unsigned char *)fname, strlen(fname), hash);
	//PRINT_MSG(VERBOSE_LEVEL,"fname %s, hash ", fname);
	
	unsigned char strhash[SHA_DIGEST_LENGTH*2];
	
	for (i=0; i < SHA_DIGEST_LENGTH; i++) {
		sprintf((char*)&(strhash[i*2]), "%02x", hash[i]);
	}
	// And print to stdout
	//PRINT_MSG(VERBOSE_LEVEL," %s (len %d)\n", strhash, strlen(strhash));
	
	//~ print_hex(hash);
	//~ PRINT_MSG(VERBOSE_LEVEL,"\n");
	
	int it = (int)(strlen((char*)strhash)/slice);
	for(i=0; i < it; i++){
		 unsigned long val;
		 memcpy(hexsubdir, &strhash[i*slice], slice*sizeof(unsigned char));
		 hexsubdir[slice] = '\0';
		 //sscanf(hexsubdir, "%x", &val);
		 val = strtol((char*)hexsubdir, NULL, 16);
		// PRINT_MSG(VERBOSE_LEVEL,"val %lu", val); //print_hex(hexsubdir);
		// PRINT_MSG(VERBOSE_LEVEL,", val %lu\n",val);
		 int len = strlen(real_file);
		 if(len == PATH_MAX)
			break;
		 snprintf(&real_file[len], PATH_MAX - len, "%lu/", val);
		//PRINT_MSG(VERBOSE_LEVEL,"b %s\n", real_file);
	}
	strcat(real_file, fname);
	PRINT_MSG(VERBOSE_LEVEL, "path %s\n", real_file);
	
	return 1;
}

#define open_wrapper(fnname)  														\
    int fnname(const char *file, int oflag, ...) {                             		\
		int (*orig_open)(const char *file, int oflag, ...);                 		\
		orig_open = dlsym(RTLD_NEXT, #fnname);										\
		PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s file name %s!\n", __func__, file); 	\
        if (oflag & O_CREAT)  {                                                		\
            va_list arg;                                                       		\
            int mask;														   		\
            va_start(arg, oflag);                                              		\
            mask = va_arg(arg, int);                                           		\
            va_end(arg); 															\
            return orig_open(file, oflag, mask);                             		\
        } else {																	\
			char real_fpath[PATH_MAX] = "\0";                                  		\
			if(translated_fname(file, real_fpath))							 		\
				return orig_open(real_fpath, oflag);								\
		}																			\
        return orig_open(file, oflag);                                       		\
    }

open_wrapper(open)
open_wrapper(open64)
#ifdef HAVE___OPEN
open_wrapper(__open)
#endif
#ifdef HAVE___OPEN64
open_wrapper(__open64)
#endif

#define open_2_wrapper(fnname)  													\
    int fnname(const char *file, int oflag) {                             			\
		int (*orig_open)(const char *file, int oflag);                 				\
		orig_open = dlsym(RTLD_NEXT, #fnname);								   		\
		PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s file name %s!\n", __func__, file); 	\
        char real_fpath[PATH_MAX] = "\0";                                  			\
		if(translated_fname(file, real_fpath))							 			\
			return orig_open(real_fpath, oflag);									\
        return orig_open(file, oflag);                                       		\
    }


#ifdef HAVE___OPEN_2
open_2_wrapper(__open_2)
#endif
#ifdef HAVE___OPEN64_2
open_2_wrapper(__open64_2)
#endif

#define openat_wrapper(fnname)  													\
    int fnname(int dirfd, const char *file, int oflag, ...) {                       \
		int (*orig_open)(int dirfd, const char *file, int oflag, ...);             	\
		orig_open = dlsym(RTLD_NEXT, #fnname);								   		\
		PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s file name %s!\n", __func__, file); 	\
        if (oflag & O_CREAT)  {                                                		\
            va_list arg;                                                            \
            mode_t mode;														    \
            va_start(arg, oflag);                                                   \
            mode = va_arg(arg, mode_t);                                 	        \
            va_end(arg); 															\
            return orig_open(dirfd, file, oflag, mode);                             \
        } else {																	\
			char real_fpath[PATH_MAX] = "\0";                                  		\
			if(translated_fname(file, real_fpath))							 	\
				return orig_open(dirfd, real_fpath, oflag);							\
		}																			\
        return orig_open(dirfd, file, oflag);                                       \
    }

openat_wrapper(openat)
openat_wrapper(openat_64)

#define openat_2_wrapper(fnname)  													\
    int fnname(int dirfd, const char *file, int oflag) {                            \
		int (*orig_open)(int dirfd, const char *file, int oflag);                 	\
		orig_open = dlsym(RTLD_NEXT, #fnname);								   		\
		PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s file name %s!\n", __func__, file); 	\
        char real_fpath[PATH_MAX] = "\0";                                      		\
		if(translated_fname(filename, real_fpath))								 	\
			return orig_open(dirfd, real_fpath, oflag);								\
        return orig_open(dirfd, file, oflag);                                       \
    }

#ifdef HAVE___OPENAT_2
openat_2_wrapper(__openat_2)
#endif
#ifdef HAVE___OPENAT64_2
openat_2_wrapper(__openat64_2)
#endif    

#define fopen_wrapper(fnname)  														\
    FILE* fnname(const char * filename, const char * mode) {                        \
		FILE* (*orig_open)(const char *, const char*);                 				\
		orig_open = dlsym(RTLD_NEXT, #fnname);								   		\
		PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s for %s!\n", __func__, filename); 	\
		char real_fpath[PATH_MAX] = "\0";                                      		\
		if(translated_fname(filename, real_fpath))								 	\
			return orig_open(real_fpath, mode);										\
        return orig_open(filename, mode);                                       	\
    }

fopen_wrapper(fopen)
fopen_wrapper(fopen64)

struct dirent *readdir(DIR *dirp)
{
	struct dirent* (*orig_readdir)(DIR *dirp);
	PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s!\n", __func__);	
	
	orig_readdir = dlsym(RTLD_NEXT, "readdir");
	if(dirp == _cur_dir.dirp){
		
		if(fread(_cur_dir.fake_dirent.d_name, sizeof(char), MAX_FILENAME_LENGTH, _cur_dir.metafile) == 0){
			PRINT_MSG(VERBOSE_LEVEL,"Reached end of dir at %ld\n", _cur_dir.loc);
			return NULL;
		}
		PRINT_MSG(VERBOSE_LEVEL,"read fname %s\n", _cur_dir.fake_dirent.d_name);
		_cur_dir.loc++;
		
		return &(_cur_dir.fake_dirent);
	}
	
	
	return orig_readdir(dirp);
}

struct dirent64 *readdir64(DIR *dirp)
{
	struct dirent64* (*orig_readdir)(DIR *dirp);
	PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s!\n", __func__);	
	orig_readdir = dlsym(RTLD_NEXT, "readdir64");
	
	if(dirp == _cur_dir.dirp){
		if(fread(_cur_dir.fake_dirent64.d_name, sizeof(char), MAX_FILENAME_LENGTH, _cur_dir.metafile) == 0){
			return NULL;
		}
		PRINT_MSG(VERBOSE_LEVEL,"read fake name %s\n", _cur_dir.fake_dirent64.d_name);
		_cur_dir.loc++;
		
		return &(_cur_dir.fake_dirent64);
	}
	
	return orig_readdir(dirp);
}


int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
	int (*orig_readdir_r)(DIR *dirp, struct dirent *entry, struct dirent **result);
	PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s!\n", __func__);
	orig_readdir_r = dlsym(RTLD_NEXT, "readdir_r");
	
	PRINT_MSG(VERBOSE_LEVEL_ERROR, "Warning: readdir_r is depricated, flat2tree_dir will ignore this call.\n");
	
	return orig_readdir_r(dirp, entry, result);
}

int readdir64_r(DIR *dirp, struct dirent64 *entry, struct dirent64 **result)
{
	int (*orig_readdir_r)(DIR *dirp, struct dirent64 *entry, struct dirent64 **result);
	PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s!\n", __func__);
	orig_readdir_r = dlsym(RTLD_NEXT, "readdir64_r");
	PRINT_MSG(VERBOSE_LEVEL_ERROR, "Warning: readdir64_r is depricated, flat2tree_dir will ignore this call.\n");

	return orig_readdir_r(dirp, entry, result);
}

#define opendir_wrapper(fnname)	\
	DIR *fnname(const char * dirname) {										\
		DIR* (*orig_opendir)(const char *);									\
		PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s %s!\n", __func__, dirname);	\
		orig_opendir = dlsym(RTLD_NEXT, #fnname);							\
		FILE *metaf;														\
		if(is_special_dir(dirname, 1, &metaf)){								\
			if(_cur_dir.dirp != NULL){										\
				PRINT_MSG(VERBOSE_LEVEL_ERROR,"Warning: opening next special 	\
						folder without closing previous\n");				\
				fclose(_cur_dir.metafile);									\
				_nest_depth = 0;											\
			}																\
			_cur_dir.dirp = orig_opendir(dirname);							\
			_cur_dir.loc = 0;												\
			_cur_dir.metafile = metaf;										\
			if(_nest_depth == 0){											\
				if(fread(&_nest_depth, sizeof(int), 1, metaf) != 1){		\
					PRINT_MSG(VERBOSE_LEVEL_ERROR, "Error reading nesting depth from %s/.flat_metadata\n", \
					dirname);																				\
				}else if(_nest_depth < 1 || _nest_depth > SHA_DIGEST_LENGTH){							   \
					PRINT_MSG(VERBOSE_LEVEL_ERROR,"Error nesting depth value should be between 1 		   \
							and %d but value is %d\n", SHA_DIGEST_LENGTH, _nest_depth);						\
				}															\
			} else 															\
				fseek(_cur_dir.metafile, sizeof(int), SEEK_SET);			\
			return _cur_dir.dirp;											\
		}																	\
		return orig_opendir(dirname);										\
	}																											

opendir_wrapper(opendir)
opendir_wrapper(opendir64)

int closedir(DIR *dirp)
{
	int (*orig_closedir)(DIR*);
	PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s!\n", __func__);
	orig_closedir = dlsym(RTLD_NEXT, "closedir");
	
	if(dirp == _cur_dir.dirp){
		_cur_dir.dirp = NULL;
		_cur_dir.loc = -1;
		fclose(_cur_dir.metafile);	
	}
	
	return orig_closedir(dirp);
}

int closedir64(DIR *dirp)
{
	int (*orig_closedir)(DIR*);
	PRINT_MSG(VERBOSE_LEVEL,"Intercepted %s!\n", __func__);
	orig_closedir = dlsym(RTLD_NEXT, "closedir64");
	
	if(dirp == _cur_dir.dirp){
		_cur_dir.dirp = NULL;
		_cur_dir.loc = -1;
		fclose(_cur_dir.metafile);	
	}
	
	return orig_closedir(dirp);
}

