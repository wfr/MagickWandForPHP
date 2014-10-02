
/* MagickWand for PHP main source file; definitions or resources, destructors, and user visible functions

   Author: Ouinnel Watson
   Homepage:
   Release Date: 2006-12-30
*/

#include "magickwand_inc.h"

/* ************************************************************************************************************** */

static MagickBooleanType MW_split_filename_on_period( char **filename, const size_t filename_len,
													  char **name_p, const int field_width, char **ext_p,
													  char **tmp_filename_p, size_t *tmp_filename_len_p )
{
	char tmp_char;
	size_t name_len;

	TSRMLS_FETCH();

/*	char *name, *ext, *tmp_filename;

	name = *name_p;
	ext = *ext_p;
	tmp_filename = *tmp_filename_p;
*/
	name_len = (size_t) (filename_len - 4);  /* name_len = length of part of filename before last '.' in a 8.3 filename */
	if ( filename_len > 4 && (*filename)[name_len] == '.' ) {
		*ext_p = &((*filename)[name_len]);  /* set *ext_p to address of '.' before extension in filename */
	}
	else {							/* the filename has no extension which we would mess with, so... */
		*ext_p = "";				/* set extension to an empty string */
		name_len = filename_len;	/* since filename has no extension, set name_len to the length of filename */
	}

	MW_DEBUG_NOTICE_EX( "*ext_p = \"%s\"", *ext_p );
	MW_DEBUG_NOTICE_EX( "name_len = %d", name_len );

	*name_p = (char *) emalloc( name_len + 1 );  /* allocate *name_p (filename before the extension); */
	if ( *name_p == (char *) NULL ) {			/* check for errors */
		MW_SPIT_FATAL_ERR( "out of memory; could not acquire memory for \"*name_p\" char* variable" );
		return MagickFalse;
	}
	/* save character at name_len index in filename (is either '.' or '\0'), then set character at name_len index in filename
	   to '\0' -- i.e., temporarily truncate filename to name_len length, so that strncpy() will end the string reliably there */
	tmp_char = (*filename)[name_len];
	(*filename)[name_len] = '\0';

	*name_p = strncpy( *name_p, *filename, name_len );  /* *name_p = start of filename to char before last '.', or entire filename (if no '.') */
	(*name_p)[name_len] = '\0';

	MW_DEBUG_NOTICE_EX( "*name_p = \"%s\"", *name_p );

	(*filename)[name_len] = tmp_char;  /* set the value of the character at filename's name_len'th index back to original value */

	/* set length of potential new image filenames to length of filename, plus size of field_width, plus length of underscore (1) */
	*tmp_filename_len_p = filename_len + (size_t) field_width + 2;

	MW_DEBUG_NOTICE_EX( "*tmp_filename_len_p = %d", *tmp_filename_len_p );

	/* allocate *tmp_filename_p (variable to hold new filename of images that do not have a filename); the "+1" accomodates the '\0' */
	*tmp_filename_p = (char *) emalloc( *tmp_filename_len_p + 1 );
	if ( *tmp_filename_p == (char *) NULL ) {
		MW_SPIT_FATAL_ERR( "out of memory; could not acquire memory for \"*tmp_filename_p\" char* variable" );
		efree( *name_p );
		return MagickFalse;
	}
	(*tmp_filename_p)[ *tmp_filename_len_p ] = '\0';
	MW_DEBUG_NOTICE( "MW_split_filename_on_period() function completed successfully" );

	return MagickTrue;
}

/* ************************************************************************************************************** */

/* Function MW_read_image() accepts a MagickWand and a potential ImageMagick pseudo-format string and checks
   for the following ImageMagick pseudo-formats:
	X:
	XC:
	MAP:
	VID:
	WIN:
	NULL:
	SCAN:
	TILE:
	LABEL:
	MAGICK:
	PLASMA:
	CAPTION:
	FRACTAL:
	PATTERN:
	STEGANO:
	GRADIENT:
	CLIPBOARD:

   If one of the pseudo-formats is found, an attempt to read it into the
   MagickWand is made. If the "VID:" or "TILE:" pseudo-formats are found, the
   rest of the format string is supposed to be a filename, and it is checked
   for PHP accessibility.

   ftp, http and https streams are handled separately.

*/
static MagickBooleanType MW_read_image( MagickWand *magick_wand, const char *format_or_file )
{
	char *colon_p,
		*magick_filename,
		real_filename[MAXPATHLEN],
		*real_magick_format,
		tmp[MW_MAX_FORMAT_NAME_LEN],
		*img_format;
	size_t real_magick_format_len;
	int	colon_idx = 0, is_magick_format = -1;
	long img_idx;
	unsigned long num_imgs;
	MagickBooleanType retval;

	TSRMLS_FETCH();

	real_filename[0] = '\0';

  /* Special check for ftp, http and https */
  if (strlen(format_or_file) >= 6) {
          if (strncasecmp(format_or_file, "ftp://", 6) == 0 ||
                  strncasecmp(format_or_file, "http://", 7) == 0 ||
                  strncasecmp(format_or_file, "https://", 8) == 0) {
                  FILE *fp;
                  php_stream *stream = php_stream_open_wrapper((char *)format_or_file, "rb", REPORT_ERRORS | ENFORCE_SAFE_MODE, NULL);

                  if (!stream) {
                          /* No stream, fail here */
                          return MagickFalse;
                  }

                  if (php_stream_can_cast(stream, PHP_STREAM_AS_STDIO | PHP_STREAM_CAST_INTERNAL) == FAILURE) {
                          php_stream_close(stream);
                          return MagickFalse;
                  }

                  if (php_stream_cast(stream, PHP_STREAM_AS_STDIO | PHP_STREAM_CAST_INTERNAL, (void*)&fp, 0) == FAILURE) {
                          php_stream_close(stream);
                          return MagickFalse;
                  }

                  if (MagickReadImageFile(magick_wand, fp)) {
                          unsigned long orig_img_idx = (unsigned long) MagickGetNumberImages( magick_wand );
                          php_stream_close(stream);

                          if ( MagickSetIteratorIndex( magick_wand, (long) orig_img_idx ) == MagickTrue ) {
                                  while ( MagickSetImageFilename( magick_wand, (char *) NULL ), MagickNextImage( magick_wand ) == MagickTrue )
                                          ;
                          }
                          MagickClearException( magick_wand );
                          MagickResetIterator( magick_wand );
                          return MagickTrue;
                  } else {
                          php_stream_close(stream);
                          MW_API_FUNC_FAIL_CHECK_WAND_ERROR_EX_1( magick_wand, MagickWand,
                                                                                                          "cannot read the format \"%s\"",
                                                                                                          format_or_file );
                          return MagickFalse;
                  }
          }
  }

#define PRV_VERIFY_MAGICK_FORMAT_FILE( magick_filename, colon_p, is_magick_format )  \
{  \
	magick_filename = colon_p + 1;  \
\
	expand_filepath(magick_filename, real_filename TSRMLS_CC);  \
	if ( real_filename[0] == '\0' || MW_FILE_FAILS_INI_TESTS( real_filename ) ) {  \
		/* PHP cannot open image with the current filename: output fatal error, with reason */  \
		zend_error( MW_E_ERROR, "%s(): PHP cannot read %s; possible php.ini restrictions",  \
								get_active_function_name(TSRMLS_C), real_filename );  \
;		return MagickFalse;  \
	}  \
	else {  \
		is_magick_format = 1;  \
	}  \
}


/*
	is_magick_format == 1	if a pseudo-format is found, and it is successfuly read
	is_magick_format == 0	if a pseudo-format is NOT found
	is_magick_format == -1	if a pseudo-format is found, but reading it fails, or
								it is set to read a file which is NOT accessible by PHP.
*/
  is_magick_format = 0;
	colon_p = strchr( format_or_file, ':' );
	if ( colon_p == (char *) NULL ) {
		expand_filepath(format_or_file, real_filename TSRMLS_CC);
	}
	else {
    colon_idx = colon_p - format_or_file;
    if ( strncasecmp( format_or_file, "X", (size_t) colon_idx ) == 0 ) {
      is_magick_format = -1;
    }
    else {
      PRV_VERIFY_MAGICK_FORMAT_FILE( magick_filename, colon_p, is_magick_format );
		}
	}

/*
	is_magick_format == 1	if a pseudo-format is found, and it is successfuly read
	is_magick_format == 0	if a pseudo-format is NOT found
	is_magick_format == -1	if a pseudo-format is found, but reading it fails, or
								it is set to read a file which is NOT accessible by PHP.
*/
	if ( is_magick_format == 1 ) {

		if ( real_filename[0] == '\0' ) {

			real_magick_format_len = (size_t) 0;

			real_magick_format = (char *) format_or_file;

			MW_DEBUG_NOTICE_EX( "The pseudo-format \"%s\" is NOT a file format", real_magick_format );
		}
		else {
			real_magick_format_len = (size_t) (MAXPATHLEN + MW_MAX_FORMAT_NAME_LEN);

			real_magick_format = (char *) ecalloc( (size_t) real_magick_format_len, sizeof( char * ) );

			if ( real_magick_format == (char *) NULL ) {
				MW_SPIT_FATAL_ERR( "could not allocate memory for array (char *)" );
				return MagickFalse;
			}

			snprintf( tmp, (colon_idx + 1), "%s", format_or_file );
			snprintf( real_magick_format, real_magick_format_len, "%s:%s", tmp, real_filename );
			MW_DEBUG_NOTICE_EX( "The pseudo-format \"%s\" IS a file format", real_magick_format );
		}

		img_idx = (long) MagickGetIteratorIndex( magick_wand );
		if ( MagickGetExceptionType(magick_wand) == UndefinedException ) {
			img_idx++;
		}
		num_imgs = (unsigned long) MagickGetNumberImages( magick_wand );

		if ( MagickReadImage( magick_wand, real_magick_format ) == MagickTrue ) {

			if ( MagickSetIteratorIndex( magick_wand, (long) img_idx ) == MagickTrue ) {

				num_imgs = ((unsigned long) MagickGetNumberImages( magick_wand )) - num_imgs - 1;

				MagickSetImageFilename( magick_wand, (char *) NULL );
				while ( num_imgs > 0 && MagickNextImage( magick_wand ) == MagickTrue ) {
					MagickSetImageFilename( magick_wand, (char *) NULL );
					num_imgs--;
				}
			}
			MagickClearException( magick_wand );

			MW_DEBUG_NOTICE_EX( "The pseudo-format \"%s\" was read successfully", real_magick_format );

			retval = MagickTrue;
		}
		else {
			/* C API cannot read the current format: output error, with reason */
			MW_API_FUNC_FAIL_CHECK_WAND_ERROR_EX_1(	magick_wand, MagickWand,
													"cannot read the format \"%s\"",
													format_or_file );

			retval = MagickFalse;
		}

		if ( real_magick_format_len > (size_t) 0  &&  real_magick_format != (char *) NULL ) {
			efree( real_magick_format );
		}
		return retval;
	}
	else {
		if ( is_magick_format == 0 ) {
			if ( real_filename[0] == '\0' || MW_FILE_FAILS_INI_TESTS( real_filename ) ) {
				zend_error( MW_E_ERROR, "%s(): PHP cannot read %s; possible php.ini restrictions",
										get_active_function_name(TSRMLS_C), real_filename );
				return MagickFalse;
			}
			else {
				MagickBooleanType set_wand_format = MagickFalse;

				if ( MagickGetNumberImages( magick_wand ) < 1 ) {
					set_wand_format = MagickTrue;
					img_idx = 0;
				}
				else {
					img_idx = (long) MagickGetIteratorIndex( magick_wand );
				}

				if ( MagickGetExceptionType(magick_wand) == UndefinedException ) {
					img_idx++;
				}

				if ( MagickReadImage( magick_wand, real_filename ) == MagickTrue ) {

					MW_DEBUG_NOTICE_EX( "The image \"%s\" was read successfully", real_filename );

					if ( MagickSetIteratorIndex( magick_wand, (long) img_idx ) == MagickTrue ) {

						if ( set_wand_format == MagickTrue ) {
							/* which means that img_idx == 0, and we are now dealing with image 0 */
							img_format = (char *) MagickGetImageFormat( magick_wand );

							if ( img_format != (char *) NULL && *img_format != '\0' && *img_format != '*' ) {
								MagickSetFormat( magick_wand, img_format );
							}
							MW_FREE_MAGICK_MEM( char *, img_format );
						}

						MagickSetImageFilename( magick_wand, (char *) NULL );
						while ( MagickNextImage( magick_wand ) == MagickTrue ) {
							MagickSetImageFilename( magick_wand, (char *) NULL );
						}
					}
					MagickClearException( magick_wand );
					return MagickTrue;
				}
				else {
					/* C API cannot read image from the current file: return, get reason with MagickGetException() */
					return MagickFalse;
				}
			}
		}
		else {
			/* is_magick_format == -1 */
			zend_error( MW_E_ERROR, "%s(): PHP cannot read %s; it specifies an unknown or disallowed ImageMagick pseudo-format",
									get_active_function_name(TSRMLS_C), format_or_file );
			return MagickFalse;
		}
	}
}

/* ************************************************************************************************************** */

/* The MagickWand C API actually MagickWriteImage()'s the current active image to a filehandle in the original
   image's format, unless the image's filename is empty, so enter the following rigamorole, which saves the image's
   current filename, then sets the image's filename to "", writes the image, and then sets the filename back to the
   saved one.
*/
static MagickBooleanType MW_write_image( MagickWand *magick_wand, const char *filename, const long img_idx )
{
	char *orig_img_filename;
	int img_filename_valid;
	char real_filename[MAXPATHLEN];

	TSRMLS_FETCH();

	real_filename[0] = '\0';
	expand_filepath(filename, real_filename TSRMLS_CC);

	if ( real_filename[0] == '\0' || MW_FILE_FAILS_INI_TESTS( real_filename ) ) {
		zend_error( MW_E_ERROR, "%s(): PHP cannot write the image at MagickWand index %ld to \"%s\"; "  \
								"possible php.ini restrictions",
								get_active_function_name(TSRMLS_C), img_idx, real_filename );
		return MagickFalse;
	}
	else {
		orig_img_filename = (char *) MagickGetImageFilename( magick_wand );
		img_filename_valid = !(orig_img_filename == (char *) NULL || *orig_img_filename == '\0');

		if ( img_filename_valid ) {
			MagickSetImageFilename( magick_wand, (char *) NULL );
		}

		MagickClearException( magick_wand );

		if ( MagickWriteImage( magick_wand, real_filename ) == MagickFalse ) {
			/* C API cannot write image to the current filename: output error, with reason */
			MW_API_FUNC_FAIL_CHECK_WAND_ERROR_EX_2(	magick_wand, MagickWand,
													"cannot write the image at MagickWand index %ld to filename \"%s\"",
													img_idx, filename
			);

			MW_FREE_MAGICK_MEM( char *, orig_img_filename );
			return MagickFalse;
		}

		if ( img_filename_valid ) {
			MagickSetImageFilename( magick_wand, orig_img_filename );
		}
		MW_FREE_MAGICK_MEM( char *, orig_img_filename );

		return MagickTrue;
	}
}

/* ************************************************************************************************************** */

/* The MagickWand C API actually MagickWriteImagesFile()'s the current active image to a filehandle in the original
   image's format, unless the image's filename is empty, so enter the following rigamorole, which saves the
   MagickWand's current filename, then sets the MagickWand's filename to "", writes the image, and then sets the
   filename back to the saved one.
*/
static MagickBooleanType MW_write_images( MagickWand *magick_wand, const char *filename )
{
	int filename_valid;
	char *orig_filename;
	char real_filename[MAXPATHLEN];

	TSRMLS_FETCH();

	real_filename[0] = '\0';
	expand_filepath(filename, real_filename TSRMLS_CC);

	if ( real_filename[0] == '\0' || MW_FILE_FAILS_INI_TESTS( real_filename ) ) {
		zend_error( MW_E_ERROR, "%s(): PHP cannot write the MagickWand's images to \"%s\"; "  \
								"possible php.ini restrictions",
								get_active_function_name(TSRMLS_C), real_filename );
		return MagickFalse;
	}
	else {
		orig_filename = (char *) MagickGetFilename( magick_wand );
		filename_valid = !(orig_filename == (char *) NULL || *orig_filename == '\0');

		if ( filename_valid ) {
			MagickSetFilename( magick_wand, (char *) NULL );
		}

		MagickClearException( magick_wand );

		if ( MagickWriteImages( magick_wand, real_filename, MagickTrue ) == MagickFalse ) {
			/* C API cannot write image to the current filename: output error, with reason */
			MW_API_FUNC_FAIL_CHECK_WAND_ERROR_EX_1(	magick_wand, MagickWand,
													"unable to write the MagickWand's images to filename \"%s\"",
													filename
			);

			MW_FREE_MAGICK_MEM( char *, orig_filename );
			return MagickFalse;
		}

		if ( filename_valid ) {
			MagickSetFilename( magick_wand, orig_filename );
		}

		MW_FREE_MAGICK_MEM( char *, orig_filename );
		return MagickTrue;
	}
}

/* ************************************************************************************************************** */


/* {{{ PHP_INI
*/

/* int OnChangeMAGICK_HOME(	zend_ini_entry *entry,
							char *new_value,
							uint new_value_length,
							void *mh_arg1,
							void *mh_arg2,
							void *mh_arg3,
							int stage TSRMLS_DC	)
*/
/*
static ZEND_INI_MH( OnChangeMAGICK_HOME )
{
	char *env_str;
	size_t env_str_size;
	zval *z_funcname, *z_env_str, *z_retval = (zval *) NULL;
	zval **z_params[1];
	int status;

	env_str_size = (size_t) strlen( new_value );

	if ( env_str_size > 0 ) {

		env_str_size += 14;		// 14 == strlen("MAGICK_HOME") + strlen("=") + 2

		env_str = (char *) ecalloc( env_str_size, sizeof(char) );

		if ( env_str == (char *) NULL ) {
			zend_error( MW_E_ERROR, "\"magickwand.magick_home\" (php.ini) update handler: PHP unable to allocate memory for an array of char (text string)" );
			return(FAILURE);
		}

		snprintf( env_str, env_str_size, "MAGICK_HOME=%s", new_value );

		MAKE_STD_ZVAL( z_funcname );
		ZVAL_STRING( z_funcname, "putenv", 1 );

		MAKE_STD_ZVAL( z_env_str);
		ZVAL_STRING( z_env_str, env_str, 1 );
		z_params[0] = &z_env_str;

		status = call_user_function_ex( EG(function_table), NULL, z_funcname, &z_retval, 1, z_params, 0, NULL TSRMLS_CC );

		zval_dtor( z_funcname );
		zval_dtor( z_env_str );

		if ( z_retval ) {
			zval_dtor( z_retval );
		}

		efree( env_str );
		if ( status == FAILURE ) {
			zend_error( MW_E_ERROR, "\"magickwand.magick_home\" (php.ini) update handler: PHP unable to set the MAGICK_HOME environment variable" );
			return(FAILURE);
		}
		else {
			zend_error( MW_E_NOTICE, "\"magickwand.magick_home\" (php.ini) update handler: SUCCESS" );
		}
	}

	return(SUCCESS);
}

ZEND_INI_BEGIN()
	ZEND_INI_ENTRY( "magickwand.magick_home", NULL, ZEND_INI_USER, OnChangeMAGICK_HOME )
ZEND_INI_END()
*/
/* }}} */


/* magickwand_functions[]	=>	Every user visible function must have an entry in magickwand_functions[].
 */
static const zend_function_entry magickwand_functions[] =
{
	PHP_FE( wandgetexception, NULL )
	PHP_FE( wandgetexceptionstring, NULL )
	PHP_FE( wandgetexceptiontype, NULL )
	PHP_FE( wandhasexception, NULL )

	PHP_FE( isdrawingwand, NULL )
	PHP_FE( ismagickwand, NULL )
	PHP_FE( ispixeliterator, NULL )
	PHP_FE( ispixelwand, NULL )


	/* DrawingWand functions */

	PHP_FE( cleardrawingwand, NULL )
	PHP_FE( clonedrawingwand, NULL )
	PHP_FE( destroydrawingwand, NULL )
	PHP_FE( drawaffine, NULL )
	PHP_FE( drawannotation, NULL )
	PHP_FE( drawarc, NULL )

	PHP_FE( drawbezier, NULL )

	PHP_FE( drawcircle, NULL )
	PHP_FE( drawcolor, NULL )
	PHP_FE( drawcomment, NULL )
	PHP_FE( drawcomposite, NULL )
	PHP_FE( drawellipse, NULL )
	PHP_FE( drawgetclippath, NULL )
	PHP_FE( drawgetcliprule, NULL )
	PHP_FE( drawgetclipunits, NULL )

	PHP_FE( drawgetexception, NULL )
	PHP_FE( drawgetexceptionstring, NULL )
	PHP_FE( drawgetexceptiontype, NULL )

	PHP_FE( drawgetfillalpha, NULL )
	PHP_FE( drawgetfillopacity, NULL )

	PHP_FE( drawgetfillcolor, NULL )
	PHP_FE( drawgetfillrule, NULL )
	PHP_FE( drawgetfont, NULL )
	PHP_FE( drawgetfontfamily, NULL )
	PHP_FE( drawgetfontsize, NULL )
	PHP_FE( drawgetfontstretch, NULL )
	PHP_FE( drawgetfontstyle, NULL )
	PHP_FE( drawgetfontweight, NULL )
	PHP_FE( drawgetgravity, NULL )

	PHP_FE( drawgetstrokealpha, NULL )
	PHP_FE( drawgetstrokeopacity, NULL )

	PHP_FE( drawgetstrokeantialias, NULL )
	PHP_FE( drawgetstrokecolor, NULL )
	PHP_FE( drawgetstrokedasharray, NULL )
	PHP_FE( drawgetstrokedashoffset, NULL )
	PHP_FE( drawgetstrokelinecap, NULL )
	PHP_FE( drawgetstrokelinejoin, NULL )
	PHP_FE( drawgetstrokemiterlimit, NULL )
	PHP_FE( drawgetstrokewidth, NULL )
	PHP_FE( drawgettextalignment, NULL )
	PHP_FE( drawgettextantialias, NULL )
	PHP_FE( drawgettextdecoration, NULL )
	PHP_FE( drawgettextencoding, NULL )

	PHP_FE( drawgetvectorgraphics, NULL )

	PHP_FE( drawgettextundercolor, NULL )
	PHP_FE( drawline, NULL )
	PHP_FE( drawmatte, NULL )
	PHP_FE( drawpathclose, NULL )
	PHP_FE( drawpathcurvetoabsolute, NULL )
	PHP_FE( drawpathcurvetorelative, NULL )
	PHP_FE( drawpathcurvetoquadraticbezierabsolute, NULL )
	PHP_FE( drawpathcurvetoquadraticbezierrelative, NULL )
	PHP_FE( drawpathcurvetoquadraticbeziersmoothabsolute, NULL )
	PHP_FE( drawpathcurvetoquadraticbeziersmoothrelative, NULL )
	PHP_FE( drawpathcurvetosmoothabsolute, NULL )
	PHP_FE( drawpathcurvetosmoothrelative, NULL )
	PHP_FE( drawpathellipticarcabsolute, NULL )
	PHP_FE( drawpathellipticarcrelative, NULL )
	PHP_FE( drawpathfinish, NULL )
	PHP_FE( drawpathlinetoabsolute, NULL )
	PHP_FE( drawpathlinetorelative, NULL )
	PHP_FE( drawpathlinetohorizontalabsolute, NULL )
	PHP_FE( drawpathlinetohorizontalrelative, NULL )
	PHP_FE( drawpathlinetoverticalabsolute, NULL )
	PHP_FE( drawpathlinetoverticalrelative, NULL )
	PHP_FE( drawpathmovetoabsolute, NULL )
	PHP_FE( drawpathmovetorelative, NULL )
	PHP_FE( drawpathstart, NULL )
	PHP_FE( drawpoint, NULL )

	PHP_FE( drawpolygon, NULL )

	PHP_FE( drawpolyline, NULL )

	PHP_FE( drawpopclippath, NULL )
	PHP_FE( drawpopdefs, NULL )
	PHP_FE( drawpoppattern, NULL )
	PHP_FE( drawpushclippath, NULL )
	PHP_FE( drawpushdefs, NULL )
	PHP_FE( drawpushpattern, NULL )
	PHP_FE( drawrectangle, NULL )
	PHP_FE( drawrender, NULL )
	PHP_FE( drawrotate, NULL )
	PHP_FE( drawroundrectangle, NULL )
	PHP_FE( drawscale, NULL )
	PHP_FE( drawsetclippath, NULL )
	PHP_FE( drawsetcliprule, NULL )
	PHP_FE( drawsetclipunits, NULL )

	PHP_FE( drawsetfillalpha, NULL )
	PHP_FE( drawsetfillopacity, NULL )

	PHP_FE( drawsetfillcolor, NULL )
	PHP_FE( drawsetfillpatternurl, NULL )
	PHP_FE( drawsetfillrule, NULL )
	PHP_FE( drawsetfont, NULL )
	PHP_FE( drawsetfontfamily, NULL )
	PHP_FE( drawsetfontsize, NULL )
	PHP_FE( drawsetfontstretch, NULL )
	PHP_FE( drawsetfontstyle, NULL )
	PHP_FE( drawsetfontweight, NULL )
	PHP_FE( drawsetgravity, NULL )

	PHP_FE( drawsetstrokealpha, NULL )
	PHP_FE( drawsetstrokeopacity, NULL )

	PHP_FE( drawsetstrokeantialias, NULL )
	PHP_FE( drawsetstrokecolor, NULL )

	PHP_FE( drawsetstrokedasharray, NULL )

	PHP_FE( drawsetstrokedashoffset, NULL )
	PHP_FE( drawsetstrokelinecap, NULL )
	PHP_FE( drawsetstrokelinejoin, NULL )
	PHP_FE( drawsetstrokemiterlimit, NULL )
	PHP_FE( drawsetstrokepatternurl, NULL )
	PHP_FE( drawsetstrokewidth, NULL )
	PHP_FE( drawsettextalignment, NULL )
	PHP_FE( drawsettextantialias, NULL )
	PHP_FE( drawsettextdecoration, NULL )
	PHP_FE( drawsettextencoding, NULL )
	PHP_FE( drawsettextundercolor, NULL )
	PHP_FE( drawsetvectorgraphics, NULL )
	PHP_FE( drawskewx, NULL )
	PHP_FE( drawskewy, NULL )
	PHP_FE( drawtranslate, NULL )
	PHP_FE( drawsetviewbox, NULL )
	PHP_FE( newdrawingwand, NULL )
	PHP_FE( popdrawingwand, NULL )
	PHP_FE( pushdrawingwand, NULL )


	/* MagickWand functions */

	PHP_FE( clearmagickwand, NULL )
	PHP_FE( clonemagickwand, NULL )
	PHP_FE( destroymagickwand, NULL )
	PHP_FE( magickadaptivethresholdimage, NULL )

/* OW modification from C API convention:
      PHP's magickaddimage adds only the current active image from one wand to another,
	  while PHP's magickaddimages replicates the functionality of the C API's
	  MagickAddImage(), adding all the images of one wnad to another.

	  Minor semantics, but original funtionality proved annoying in actual usage.
*/
	PHP_FE( magickaddimage, NULL )
	PHP_FE( magickaddimages, NULL )

	PHP_FE( magickaddnoiseimage, NULL )
	PHP_FE( magickaffinetransformimage, NULL )
	PHP_FE( magickannotateimage, NULL )
	PHP_FE( magickappendimages, NULL )
	PHP_FE( magickaverageimages, NULL )
	PHP_FE( magickblackthresholdimage, NULL )

	PHP_FE( magickblurimage, NULL )
/*	PHP_FE( magickblurimagechannel, NULL )  */

	PHP_FE( magickborderimage, NULL )
	PHP_FE( magickcharcoalimage, NULL )
	PHP_FE( magickchopimage, NULL )
	PHP_FE( magickclipimage, NULL )
	PHP_FE( magickclippathimage, NULL )
	PHP_FE( magickcoalesceimages, NULL )
	PHP_FE( magickcolorfloodfillimage, NULL )
	PHP_FE( magickcolorizeimage, NULL )
	PHP_FE( magickcombineimages, NULL )
	PHP_FE( magickcommentimage, NULL )

	PHP_FE( magickcompareimages, NULL )
/*	PHP_FE( magickcompareimagechannels, NULL )  */

	PHP_FE( magickcompositeimage, NULL )
	PHP_FE( magickconstituteimage, NULL )
	PHP_FE( magickcontrastimage, NULL )

	PHP_FE( magickconvolveimage, NULL )
/*	PHP_FE( magickconvolveimagechannel, NULL )  */

	PHP_FE( magickcropimage, NULL )
	PHP_FE( magickcyclecolormapimage, NULL )
	PHP_FE( magickdeconstructimages, NULL )

	PHP_FE( magickdescribeimage, NULL )
	PHP_FE( magickidentifyimage, NULL )

	PHP_FE( magickdespeckleimage, NULL )

	PHP_FE( magickdisplayimage, NULL )
	PHP_FE( magickdisplayimages, NULL )

	PHP_FE( magickdrawimage, NULL )

	PHP_FE( magickechoimageblob, NULL )
	PHP_FE( magickechoimagesblob, NULL )

	PHP_FE( magickedgeimage, NULL )
	PHP_FE( magickembossimage, NULL )
	PHP_FE( magickenhanceimage, NULL )
	PHP_FE( magickequalizeimage, NULL )

	PHP_FE( magickevaluateimage, NULL )
/*	PHP_FE( magickevaluateimagechannel, NULL )  */

	PHP_FE( magickflattenimages, NULL )
	PHP_FE( magickflipimage, NULL )
	PHP_FE( magickflopimage, NULL )
	PHP_FE( magickframeimage, NULL )

	PHP_FE( magickfximage, NULL )
/*	PHP_FE( magickfximagechannel, NULL )  */

	PHP_FE( magickgammaimage, NULL )
/*	PHP_FE( magickgammaimagechannel, NULL )  */

	PHP_FE( magickgaussianblurimage, NULL )
/*	PHP_FE( magickgaussianblurimagechannel, NULL )  */

	PHP_FE( magickgetcompression, NULL )
	PHP_FE( magickgetcompressionquality, NULL )

	PHP_FE( magickgetcopyright, NULL )

	PHP_FE( magickgetexception, NULL )
	PHP_FE( magickgetexceptionstring, NULL )
	PHP_FE( magickgetexceptiontype, NULL )

	PHP_FE( magickgetfilename, NULL )
	PHP_FE( magickgetformat, NULL )
	PHP_FE( magickgethomeurl, NULL )
	PHP_FE( magickgetimage, NULL )
	PHP_FE( magickgetimagebackgroundcolor, NULL )
	PHP_FE( magickgetimageblob, NULL )
	PHP_FE( magickgetimagesblob, NULL )
	PHP_FE( magickgetimageblueprimary, NULL )
	PHP_FE( magickgetimagebordercolor, NULL )

	PHP_FE( magickgetimagechannelmean, NULL )

//	PHP_FE( magickgetimagechannelstatistics, NULL )

	PHP_FE( magickgetimagecolormapcolor, NULL )
	PHP_FE( magickgetimagecolors, NULL )
	PHP_FE( magickgetimagecolorspace, NULL )
	PHP_FE( magickgetimagecompose, NULL )
	PHP_FE( magickgetimagecompression, NULL )
	PHP_FE( magickgetimagecompressionquality, NULL )
	PHP_FE( magickgetimagedelay, NULL )

	PHP_FE( magickgetimagedepth, NULL )
/*	PHP_FE( magickgetimagechanneldepth, NULL )  */

	PHP_FE( magickgetimagedistortion, NULL )
/*	PHP_FE( magickgetimagechanneldistortion, NULL )	*/

	PHP_FE( magickgetimagedispose, NULL )
	PHP_FE( magickgetimageendian, NULL )
	PHP_FE( magickgetimageattribute, NULL )

	PHP_FE( magickgetimageextrema, NULL )
/*	PHP_FE( magickgetimagechannelextrema, NULL )  */

	PHP_FE( magickgetimagefilename, NULL )
	PHP_FE( magickgetimageformat, NULL )
	PHP_FE( magickgetimagegamma, NULL )
	PHP_FE( magickgetimagegreenprimary, NULL )
	PHP_FE( magickgetimageheight, NULL )
	PHP_FE( magickgetimagehistogram, NULL )
	PHP_FE( magickgetimageindex, NULL )
	PHP_FE( magickgetimageinterlacescheme, NULL )
	PHP_FE( magickgetimageiterations, NULL )
	PHP_FE( magickgetimagemattecolor, NULL )

	PHP_FE( magickgetimagemimetype, NULL )
	PHP_FE( magickgetmimetype, NULL )

	PHP_FE( magickgetimagepage, NULL )

	PHP_FE( magickgetimagepixelcolor, NULL )

	PHP_FE( magickgetimagepixels, NULL )
	PHP_FE( magickgetimageprofile, NULL )
	PHP_FE( magickgetimageproperty, NULL )
	PHP_FE( magickgetimageredprimary, NULL )

	PHP_FE( magickgetimageregion, NULL )

	PHP_FE( magickgetimagerenderingintent, NULL )
	PHP_FE( magickgetimageresolution, NULL )
	PHP_FE( magickgetimagescene, NULL )
	PHP_FE( magickgetimagesignature, NULL )
	PHP_FE( magickgetimagesize, NULL )

	PHP_FE( magickgetimagetickspersecond, NULL )

	PHP_FE( magickgetimagetotalinkdensity, NULL )

	PHP_FE( magickgetimagetype, NULL )
	PHP_FE( magickgetimageunits, NULL )
	PHP_FE( magickgetimagevirtualpixelmethod, NULL )
	PHP_FE( magickgetimagewhitepoint, NULL )
	PHP_FE( magickgetimagewidth, NULL )
	PHP_FE( magickgetinterlacescheme, NULL )
	PHP_FE( magickgetnumberimages, NULL )

	PHP_FE( magickgetoption, NULL )

	PHP_FE( magickgetpackagename, NULL )

	PHP_FE( magickgetpage, NULL )

	PHP_FE( magickgetquantumdepth, NULL )

	PHP_FE( magickgetquantumrange, NULL )

	PHP_FE( magickgetreleasedate, NULL )

	PHP_FE( magickgetresource, NULL )

	PHP_FE( magickgetresourcelimit, NULL )
	PHP_FE( magickgetsamplingfactors, NULL )

/*	PHP_FE( magickgetsize, NULL ) */
	ZEND_FALIAS( magickgetsize, magickgetwandsize, NULL )
	PHP_FE( magickgetwandsize, NULL )

	PHP_FE( magickgetversion, NULL )

/* OW added for convenience in getting the version as a number or string */
	PHP_FE( magickgetversionnumber, NULL )
	PHP_FE( magickgetversionstring, NULL )
/* end convenience functions */

	PHP_FE( magickhasnextimage, NULL )
	PHP_FE( magickhaspreviousimage, NULL )
	PHP_FE( magickimplodeimage, NULL )
	PHP_FE( magicklabelimage, NULL )

	PHP_FE( magicklevelimage, NULL )
/*	PHP_FE( magicklevelimagechannel, NULL )  */

	PHP_FE( magickmagnifyimage, NULL )
	PHP_FE( magickmapimage, NULL )
	PHP_FE( magickmattefloodfillimage, NULL )
	PHP_FE( magickmedianfilterimage, NULL )
	PHP_FE( magickminifyimage, NULL )
	PHP_FE( magickmodulateimage, NULL )
	PHP_FE( magickmontageimage, NULL )
	PHP_FE( magickmorphimages, NULL )
	PHP_FE( magickmosaicimages, NULL )
	PHP_FE( magickmotionblurimage, NULL )

	PHP_FE( magicknegateimage, NULL )
/*	PHP_FE( magicknegateimagechannel, NULL )  */

	PHP_FE( magicknewimage, NULL )

	PHP_FE( magicknextimage, NULL )
	PHP_FE( magicknormalizeimage, NULL )
	PHP_FE( magickoilpaintimage, NULL )

	PHP_FE( magickfloodfillpaintimage, NULL )
	PHP_FE( magickopaquepaintimage, NULL )
	PHP_FE( magicktransparentpaintimage, NULL )

	PHP_FE( magickpingimage, NULL )
	PHP_FE( magickposterizeimage, NULL )
	PHP_FE( magickpreviewimages, NULL )
	PHP_FE( magickpreviousimage, NULL )
	PHP_FE( magickprofileimage, NULL )
	PHP_FE( magickquantizeimage, NULL )
	PHP_FE( magickquantizeimages, NULL )
	PHP_FE( magickqueryconfigureoption, NULL )
	PHP_FE( magickqueryconfigureoptions, NULL )
	PHP_FE( magickqueryfontmetrics, NULL )

/* the following functions are convenience functions I implemented, as alternatives to calling MagickQueryFontMetrics */
	PHP_FE( magickgetcharwidth, NULL )
	PHP_FE( magickgetcharheight, NULL )
	PHP_FE( magickgettextascent, NULL )
	PHP_FE( magickgettextdescent, NULL )
	PHP_FE( magickgetstringwidth, NULL )
	PHP_FE( magickgetstringheight, NULL )
	PHP_FE( magickgetmaxtextadvance, NULL )
/* end of convenience functions */

	PHP_FE( magickqueryfonts, NULL )
	PHP_FE( magickqueryformats, NULL )
	PHP_FE( magickradialblurimage, NULL )
	PHP_FE( magickraiseimage, NULL )
	PHP_FE( magickreadimage, NULL )
	PHP_FE( magickreadimageblob, NULL )
	PHP_FE( magickreadimagefile, NULL )

	/* Custom PHP function; accepts a PHP array of filenames and attempts to read them all into the MagickWand */
	PHP_FE( magickreadimages, NULL )

	PHP_FE( magickrecolorimage, NULL )
	PHP_FE( magickreducenoiseimage, NULL )
	PHP_FE( magickremoveimage, NULL )
	PHP_FE( magickremoveimageprofile, NULL )

	PHP_FE( magickremoveimageprofiles, NULL )

	PHP_FE( magickresetimagepage, NULL )
	PHP_FE( magickresetiterator, NULL )
	PHP_FE( magickresampleimage, NULL )
	PHP_FE( magickresizeimage, NULL )
	PHP_FE( magickrollimage, NULL )
	PHP_FE( magickrotateimage, NULL )
	PHP_FE( magicksampleimage, NULL )
	PHP_FE( magickscaleimage, NULL )
	PHP_FE( magickseparateimagechannel, NULL )

	PHP_FE( magicksepiatoneimage, NULL )

	PHP_FE( magicksetbackgroundcolor, NULL )

	PHP_FE( magicksetcompression, NULL )

	PHP_FE( magicksetcompressionquality, NULL )
	PHP_FE( magicksetfilename, NULL )
	PHP_FE( magicksetfirstiterator, NULL )
	PHP_FE( magicksetformat, NULL )

	PHP_FE( magicksetimage, NULL )

	PHP_FE( magicksetimagealphachannel, NULL )
	PHP_FE( magicksetimageattribute, NULL )

	PHP_FE( magicksetimagebackgroundcolor, NULL )
	PHP_FE( magicksetimagebias, NULL )
	PHP_FE( magicksetimageblueprimary, NULL )
	PHP_FE( magicksetimagebordercolor, NULL )
	PHP_FE( magicksetimagecolormapcolor, NULL )
	PHP_FE( magicksetimagecolorspace, NULL )
	PHP_FE( magicksetimagecompose, NULL )
	PHP_FE( magicksetimagecompression, NULL )
	PHP_FE( magicksetimagecompressionquality, NULL )
	PHP_FE( magicksetimagedelay, NULL )

	PHP_FE( magicksetimagedepth, NULL )
/*	PHP_FE( magicksetimagechanneldepth, NULL )  */

	PHP_FE( magicksetimagedispose, NULL )
	PHP_FE( magicksetimageendian, NULL )

	PHP_FE( magicksetimageextent, NULL )
	ZEND_FALIAS( magicksetimagesize, magicksetimageextent, NULL )

	PHP_FE( magicksetimagefilename, NULL )

	PHP_FE( magicksetimageformat, NULL )

	PHP_FE( magicksetimagegamma, NULL )
	PHP_FE( magicksetimagegreenprimary, NULL )
	PHP_FE( magicksetimageindex, NULL )
	PHP_FE( magicksetimageinterlacescheme, NULL )
	PHP_FE( magicksetimageiterations, NULL )
	PHP_FE( magicksetimagemattecolor, NULL )
	PHP_FE( magicksetimageoption, NULL )

	PHP_FE( magicksetimagepage, NULL )

	PHP_FE( magicksetimagepixels, NULL )
	PHP_FE( magicksetimageprofile, NULL )
	PHP_FE( magicksetimageproperty, NULL )
	PHP_FE( magicksetimageredprimary, NULL )
	PHP_FE( magicksetimagerenderingintent, NULL )
	PHP_FE( magicksetimageresolution, NULL )
	PHP_FE( magicksetimagescene, NULL )

	PHP_FE( magicksetimagetickspersecond, NULL )

	PHP_FE( magicksetimagetype, NULL )
	PHP_FE( magicksetimageunits, NULL )
	PHP_FE( magicksetimagevirtualpixelmethod, NULL )
	PHP_FE( magicksetimagewhitepoint, NULL )

	PHP_FE( magicksetinterlacescheme, NULL )
	PHP_FE( magicksetlastiterator, NULL )

	PHP_FE( magicksetoption, NULL )

	PHP_FE( magicksetpage, NULL )

	PHP_FE( magicksetpassphrase, NULL )
	PHP_FE( magicksetresolution, NULL )
	PHP_FE( magicksetresourcelimit, NULL )
	PHP_FE( magicksetsamplingfactors, NULL )

	PHP_FE( magicksetsize, NULL )
	ZEND_FALIAS( magicksetwandsize, magicksetsize, NULL )

	PHP_FE( magicksettype, NULL )

	PHP_FE( magickshadowimage, NULL )

	PHP_FE( magicksharpenimage, NULL )
/*	PHP_FE( magicksharpenimagechannel, NULL ) */

	PHP_FE( magickshaveimage, NULL )
	PHP_FE( magickshearimage, NULL )
	PHP_FE( magicksolarizeimage, NULL )
	PHP_FE( magickspliceimage, NULL )
	PHP_FE( magickspreadimage, NULL )
	PHP_FE( magickstatisticimage, NULL )
	PHP_FE( magicksteganoimage, NULL )
	PHP_FE( magickstereoimage, NULL )
	PHP_FE( magickstripimage, NULL )
	PHP_FE( magickswirlimage, NULL )
	PHP_FE( magicktextureimage, NULL )

	PHP_FE( magickthresholdimage, NULL )
/*	PHP_FE( magickthresholdimagechannel, NULL ) */

	PHP_FE( magickthumbnailimage, NULL )

	PHP_FE( magicktintimage, NULL )
	PHP_FE( magicktransformimage, NULL )
	PHP_FE( magicktrimimage, NULL )

	PHP_FE( magickunsharpmaskimage, NULL )
/*	PHP_FE( magickunsharpmaskimagechannel, NULL ) */

	PHP_FE( magickwaveimage, NULL )
	PHP_FE( magickwhitethresholdimage, NULL )
	PHP_FE( magickwriteimage, NULL )
	PHP_FE( magickwriteimagefile, NULL )
	PHP_FE( magickwriteimages, NULL )
	PHP_FE( newmagickwand, NULL )

	/* PixelIterator functions */

	PHP_FE( clearpixeliterator, NULL )
	PHP_FE( destroypixeliterator, NULL )
	PHP_FE( newpixeliterator, NULL )
	PHP_FE( newpixelregioniterator, NULL )

	PHP_FE( pixelgetiteratorexception, NULL )
	PHP_FE( pixelgetiteratorexceptionstring, NULL )
	PHP_FE( pixelgetiteratorexceptiontype, NULL )

	PHP_FE( pixelgetnextiteratorrow, NULL )

	PHP_FE( pixelgetpreviousiteratorrow, NULL )

	PHP_FE( pixelresetiterator, NULL )

	PHP_FE( pixelsetfirstiteratorrow, NULL )

	PHP_FE( pixelsetiteratorrow, NULL )

	PHP_FE( pixelsetlastiteratorrow, NULL )

	PHP_FE( pixelsynciterator, NULL )


	/* PixelWand functions */

	PHP_FE( clearpixelwand, NULL )

	PHP_FE( clonepixelwand, NULL )

	PHP_FE( destroypixelwand, NULL )

/************************** OW_Modified *************************
MagickWand's destroypixelwandarray() == ImageMagick's destroypixelwands
*/
/*	PHP_FE( destroypixelwands, NULL ) */
	ZEND_FALIAS( destroypixelwands, destroypixelwandarray, NULL )

	PHP_FE( destroypixelwandarray, NULL )

	PHP_FE( ispixelwandsimilar, NULL )

	PHP_FE( newpixelwand, NULL )

/************************** OW_Modified *************************
MagickWand's newpixelwandarray() == ImageMagick's newpixelwands
*/
	ZEND_FALIAS( newpixelwands, newpixelwandarray, NULL )
	PHP_FE( newpixelwandarray, NULL )

	PHP_FE( pixelgetalpha, NULL )
	PHP_FE( pixelgetalphaquantum, NULL )

	PHP_FE( pixelgetexception, NULL )
	PHP_FE( pixelgetexceptionstring, NULL )
	PHP_FE( pixelgetexceptiontype, NULL )

	PHP_FE( pixelgetblack, NULL )
	PHP_FE( pixelgetblackquantum, NULL )
	PHP_FE( pixelgetblue, NULL )
	PHP_FE( pixelgetbluequantum, NULL )
	PHP_FE( pixelgetcolorasstring, NULL )
	PHP_FE( pixelgetcolorcount, NULL )
	PHP_FE( pixelgetcyan, NULL )
	PHP_FE( pixelgetcyanquantum, NULL )
	PHP_FE( pixelgetgreen, NULL )
	PHP_FE( pixelgetgreenquantum, NULL )
	PHP_FE( pixelgetindex, NULL )
	PHP_FE( pixelgetmagenta, NULL )
	PHP_FE( pixelgetmagentaquantum, NULL )
	PHP_FE( pixelgetopacity, NULL )
	PHP_FE( pixelgetopacityquantum, NULL )
	PHP_FE( pixelgetquantumcolor, NULL )
	PHP_FE( pixelgetred, NULL )
	PHP_FE( pixelgetredquantum, NULL )
	PHP_FE( pixelgetyellow, NULL )
	PHP_FE( pixelgetyellowquantum, NULL )
	PHP_FE( pixelsetalpha, NULL )
	PHP_FE( pixelsetalphaquantum, NULL )
	PHP_FE( pixelsetblack, NULL )
	PHP_FE( pixelsetblackquantum, NULL )
	PHP_FE( pixelsetblue, NULL )
	PHP_FE( pixelsetbluequantum, NULL )
	PHP_FE( pixelsetcolor, NULL )
	PHP_FE( pixelsetcolorcount, NULL )
	PHP_FE( pixelsetcyan, NULL )
	PHP_FE( pixelsetcyanquantum, NULL )
	PHP_FE( pixelsetgreen, NULL )
	PHP_FE( pixelsetgreenquantum, NULL )
	PHP_FE( pixelsetindex, NULL )
	PHP_FE( pixelsetmagenta, NULL )
	PHP_FE( pixelsetmagentaquantum, NULL )
	PHP_FE( pixelsetopacity, NULL )
	PHP_FE( pixelsetopacityquantum, NULL )
	PHP_FE( pixelsetquantumcolor, NULL )
	PHP_FE( pixelsetred, NULL )
	PHP_FE( pixelsetredquantum, NULL )
	PHP_FE( pixelsetyellow, NULL )
	PHP_FE( pixelsetyellowquantum, NULL )

	{NULL, NULL, NULL}	/* Must be the last line in magickwand_functions[] */
};

/* magickwand_module_entry
 */
zend_module_entry magickwand_module_entry =
{
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"magickwand",
	magickwand_functions,
	PHP_MINIT( magickwand ),
	PHP_MSHUTDOWN( magickwand ),
	NULL,	/* ZEND_RINIT( magickwand ),		*/	/* Replace with NULL if there's nothing to do at request start */
	NULL,	/* ZEND_RSHUTDOWN( magickwand ),	*/	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO( magickwand ),
#if ZEND_MODULE_API_NO >= 20010901
	MAGICKWAND_VERSION, /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MAGICKWAND
	ZEND_GET_MODULE( magickwand )
#endif

/* {{{ DrawingWand_destruction_handler
*/
static void DrawingWand_destruction_handler( zend_rsrc_list_entry *rsrc TSRMLS_DC )
{
	MW_DEBUG_NOTICE( "Entered DrawingWand destruction handler" );
	MW_DESTROY_WAND_RSRC( DrawingWand, (rsrc->ptr) );
}
/* }}} */

/* {{{ MagickWand_destruction_handler
*/
static void MagickWand_destruction_handler( zend_rsrc_list_entry *rsrc TSRMLS_DC )
{
	MW_DEBUG_NOTICE( "Entered MagickWand destruction handler" );
	MW_DESTROY_WAND_RSRC( MagickWand, (rsrc->ptr) );
}
/* }}} */

/* {{{ PixelIterator_destruction_handler
*/
static void PixelIterator_destruction_handler( zend_rsrc_list_entry *rsrc TSRMLS_DC )
{
	MW_DEBUG_NOTICE( "Entered PixelIterator destruction handler" );
	MW_DESTROY_WAND_RSRC( PixelIterator, (rsrc->ptr) );
}
/* }}} */

/* {{{ PixelWand_destruction_handler
*/
static void PixelWand_destruction_handler( zend_rsrc_list_entry *rsrc TSRMLS_DC )
{
	MW_DEBUG_NOTICE( "Entered PixelWand destruction handler" );
	MW_DESTROY_WAND_RSRC( PixelWand, (rsrc->ptr) );
}
/* }}} */

/* {{{ PixelIteratorPixelWand_destruction_handler
*/
static void PixelIteratorPixelWand_destruction_handler( zend_rsrc_list_entry *rsrc TSRMLS_DC )
{
/*  NOTE: the reason this method does nothing and returns, is due to the fact that the resource
	this function is supposed to destroy is a PixelWand that was included as part of an array
	of PixelWands that were returned by PixelGetNextIteratorRow( pixel_iterator ). The
	particular PixelWand that this function is supposed to destroy, (via the same method as
	PixelWand_destruction_handler() above), is actually DIRECTLY LINKED to the PixelIterator
	the array it was in came from.
	(This, I realized, is because a PixelIterator's PixelWands are ONLY created once, and the
	array they are in is a set size, and when you request different rows of an image, you
	actually get the exact same PixelWands, just with different colors, repesenting the colors
	of the requested row of the image.)
	As a result, destroying it now, would cause ImageMagick to try to destroy it twice, once
	now, and once when the PixelIterator_destruction_handler() above is called to destroy the
	PixelIterator it actually belongs to.
	This causes ImageMagick to cause a KING KAH-MEHA-MEHA SIZED ERROR, (since it will not
	Destroy what does not exist, i.e. what it has already destroyed) which aborts the entire
	program, (via an assert() call), in the middle of PHP script execution.

	So, enter the PixelIteratorPixelWand "type", and it's associated handler, which does
	absolutely nothing!
*/
	MW_DEBUG_NOTICE( "Entered PixelIteratorPixelWand destruction handler (doing nothing...)" );
	return;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
*/
PHP_MINIT_FUNCTION( magickwand )
{
	unsigned long range_num;

/*  REGISTER_INI_ENTRIES();
*/
	/* Persist the MagickWand environment */
	MagickWandGenesis();

/*	MagickSetResourceLimit(MemoryResource, (unsigned long) INI_INT("memory_limit"));
*/
	if (MagickSetResourceLimit(MemoryResource, 8) == MagickFalse) {
		zend_error(MW_E_WARNING, "The MagickWand environment's memory resource limit could not be set!");
	}

	MW_DEFINE_PHP_RSRC( DrawingWand   );
	MW_DEFINE_PHP_RSRC( MagickWand    );
	MW_DEFINE_PHP_RSRC( PixelIterator );
	MW_DEFINE_PHP_RSRC( PixelWand     );

	MW_DEFINE_PHP_RSRC( PixelIteratorPixelWand );

	/* ImageMagick Quantum color constants, registered as double constants in PHP,
	   since they are based on the Quantum datatype, which could be the size of an unsigned long,
	   which PHP does not support
	*/
	(void *) MagickGetQuantumRange( &range_num );
	MW_QuantumRange = (double) range_num;

	REGISTER_DOUBLE_CONSTANT( "MW_QuantumRange",       MW_QuantumRange, CONST_PERSISTENT );
	REGISTER_DOUBLE_CONSTANT( "MW_MaxRGB",             MW_QuantumRange, CONST_PERSISTENT );
	REGISTER_DOUBLE_CONSTANT( "MW_OpaqueOpacity",      0,               CONST_PERSISTENT );
	REGISTER_DOUBLE_CONSTANT( "MW_TransparentOpacity", MW_QuantumRange, CONST_PERSISTENT );

	/***** Alphachannel *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedAlphaChannel );
	MW_REGISTER_LONG_CONSTANT( ActivateAlphaChannel );
	MW_REGISTER_LONG_CONSTANT( DeactivateAlphaChannel );
	MW_REGISTER_LONG_CONSTANT( ResetAlphaChannel );
	MW_REGISTER_LONG_CONSTANT( SetAlphaChannel );

	/***** AlignType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedAlign );
	MW_REGISTER_LONG_CONSTANT( LeftAlign );
	MW_REGISTER_LONG_CONSTANT( CenterAlign );
	MW_REGISTER_LONG_CONSTANT( RightAlign );

	/***** ChannelType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedChannel );
	MW_REGISTER_LONG_CONSTANT( RedChannel );
	MW_REGISTER_LONG_CONSTANT( CyanChannel );
	MW_REGISTER_LONG_CONSTANT( GreenChannel );
	MW_REGISTER_LONG_CONSTANT( MagentaChannel );
	MW_REGISTER_LONG_CONSTANT( BlueChannel );
	MW_REGISTER_LONG_CONSTANT( YellowChannel );
	MW_REGISTER_LONG_CONSTANT( AlphaChannel );
	MW_REGISTER_LONG_CONSTANT( OpacityChannel );
	MW_REGISTER_LONG_CONSTANT( BlackChannel );
	MW_REGISTER_LONG_CONSTANT( IndexChannel );
	MW_REGISTER_LONG_CONSTANT( AllChannels );

	/***** ClipPathUnits *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedPathUnits );
	MW_REGISTER_LONG_CONSTANT( UserSpace );
	MW_REGISTER_LONG_CONSTANT( UserSpaceOnUse );
	MW_REGISTER_LONG_CONSTANT( ObjectBoundingBox );

	/***** ColorspaceType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedColorspace );
	MW_REGISTER_LONG_CONSTANT( RGBColorspace );
	MW_REGISTER_LONG_CONSTANT( GRAYColorspace );
	MW_REGISTER_LONG_CONSTANT( TransparentColorspace );
	MW_REGISTER_LONG_CONSTANT( OHTAColorspace );
	MW_REGISTER_LONG_CONSTANT( LABColorspace );
	MW_REGISTER_LONG_CONSTANT( XYZColorspace );
	MW_REGISTER_LONG_CONSTANT( YCbCrColorspace );
	MW_REGISTER_LONG_CONSTANT( YCCColorspace );
	MW_REGISTER_LONG_CONSTANT( YIQColorspace );
	MW_REGISTER_LONG_CONSTANT( YPbPrColorspace );
	MW_REGISTER_LONG_CONSTANT( YUVColorspace );
	MW_REGISTER_LONG_CONSTANT( CMYKColorspace );
	MW_REGISTER_LONG_CONSTANT( sRGBColorspace );
	MW_REGISTER_LONG_CONSTANT( HSBColorspace );
	MW_REGISTER_LONG_CONSTANT( HSLColorspace );
	MW_REGISTER_LONG_CONSTANT( HWBColorspace );

	/***** CompositeOperator *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedCompositeOp );
	MW_REGISTER_LONG_CONSTANT( NoCompositeOp );
	MW_REGISTER_LONG_CONSTANT( AddCompositeOp );
	MW_REGISTER_LONG_CONSTANT( AtopCompositeOp );
	MW_REGISTER_LONG_CONSTANT( BlendCompositeOp );
	MW_REGISTER_LONG_CONSTANT( BumpmapCompositeOp );
	MW_REGISTER_LONG_CONSTANT( ClearCompositeOp );
	MW_REGISTER_LONG_CONSTANT( ColorBurnCompositeOp );
	MW_REGISTER_LONG_CONSTANT( ColorDodgeCompositeOp );
	MW_REGISTER_LONG_CONSTANT( ColorizeCompositeOp );
	MW_REGISTER_LONG_CONSTANT( CopyBlackCompositeOp );
	MW_REGISTER_LONG_CONSTANT( CopyBlueCompositeOp );
	MW_REGISTER_LONG_CONSTANT( CopyCompositeOp );
	MW_REGISTER_LONG_CONSTANT( CopyCyanCompositeOp );
	MW_REGISTER_LONG_CONSTANT( CopyGreenCompositeOp );
	MW_REGISTER_LONG_CONSTANT( CopyMagentaCompositeOp );
	MW_REGISTER_LONG_CONSTANT( CopyOpacityCompositeOp );
	MW_REGISTER_LONG_CONSTANT( CopyRedCompositeOp );
	MW_REGISTER_LONG_CONSTANT( CopyYellowCompositeOp );
	MW_REGISTER_LONG_CONSTANT( DarkenCompositeOp );
	MW_REGISTER_LONG_CONSTANT( DstAtopCompositeOp );
	MW_REGISTER_LONG_CONSTANT( DstCompositeOp );
	MW_REGISTER_LONG_CONSTANT( DstInCompositeOp );
	MW_REGISTER_LONG_CONSTANT( DstOutCompositeOp );
	MW_REGISTER_LONG_CONSTANT( DstOverCompositeOp );
	MW_REGISTER_LONG_CONSTANT( DifferenceCompositeOp );
	MW_REGISTER_LONG_CONSTANT( DisplaceCompositeOp );
	MW_REGISTER_LONG_CONSTANT( DissolveCompositeOp );
	MW_REGISTER_LONG_CONSTANT( ExclusionCompositeOp );
	MW_REGISTER_LONG_CONSTANT( HardLightCompositeOp );
	MW_REGISTER_LONG_CONSTANT( HueCompositeOp );
	MW_REGISTER_LONG_CONSTANT( InCompositeOp );
	MW_REGISTER_LONG_CONSTANT( LightenCompositeOp );
	MW_REGISTER_LONG_CONSTANT( LuminizeCompositeOp );
	MW_REGISTER_LONG_CONSTANT( MinusCompositeOp );
	MW_REGISTER_LONG_CONSTANT( ModulateCompositeOp );
	MW_REGISTER_LONG_CONSTANT( MultiplyCompositeOp );
	MW_REGISTER_LONG_CONSTANT( OutCompositeOp );
	MW_REGISTER_LONG_CONSTANT( OverCompositeOp );
	MW_REGISTER_LONG_CONSTANT( OverlayCompositeOp );
	MW_REGISTER_LONG_CONSTANT( PlusCompositeOp );
	MW_REGISTER_LONG_CONSTANT( ReplaceCompositeOp );
	MW_REGISTER_LONG_CONSTANT( SaturateCompositeOp );
	MW_REGISTER_LONG_CONSTANT( ScreenCompositeOp );
	MW_REGISTER_LONG_CONSTANT( SoftLightCompositeOp );
	MW_REGISTER_LONG_CONSTANT( SrcAtopCompositeOp );
	MW_REGISTER_LONG_CONSTANT( SrcCompositeOp );
	MW_REGISTER_LONG_CONSTANT( SrcInCompositeOp );
	MW_REGISTER_LONG_CONSTANT( SrcOutCompositeOp );
	MW_REGISTER_LONG_CONSTANT( SrcOverCompositeOp );
	MW_REGISTER_LONG_CONSTANT( SubtractCompositeOp );
	MW_REGISTER_LONG_CONSTANT( ThresholdCompositeOp );
	MW_REGISTER_LONG_CONSTANT( XorCompositeOp );

	/***** CompressionType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedCompression );
	MW_REGISTER_LONG_CONSTANT( NoCompression );
	MW_REGISTER_LONG_CONSTANT( BZipCompression );
	MW_REGISTER_LONG_CONSTANT( FaxCompression );
	MW_REGISTER_LONG_CONSTANT( Group4Compression );
	MW_REGISTER_LONG_CONSTANT( JPEGCompression );
	MW_REGISTER_LONG_CONSTANT( LosslessJPEGCompression );
	MW_REGISTER_LONG_CONSTANT( LZWCompression );
	MW_REGISTER_LONG_CONSTANT( RLECompression );
	MW_REGISTER_LONG_CONSTANT( ZipCompression );

	/***** DecorationType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedDecoration );
	MW_REGISTER_LONG_CONSTANT( NoDecoration );
	MW_REGISTER_LONG_CONSTANT( UnderlineDecoration );
	MW_REGISTER_LONG_CONSTANT( OverlineDecoration );
	MW_REGISTER_LONG_CONSTANT( LineThroughDecoration );

	/***** DisposeType *****/
	MW_REGISTER_LONG_CONSTANT( UnrecognizedDispose );
	MW_REGISTER_LONG_CONSTANT( UndefinedDispose );
	MW_REGISTER_LONG_CONSTANT( NoneDispose );
	MW_REGISTER_LONG_CONSTANT( BackgroundDispose );
	MW_REGISTER_LONG_CONSTANT( PreviousDispose );

	/***** ExceptionType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedException );
	MW_REGISTER_LONG_CONSTANT( WarningException );
	MW_REGISTER_LONG_CONSTANT( ResourceLimitWarning );
	MW_REGISTER_LONG_CONSTANT( TypeWarning );
	MW_REGISTER_LONG_CONSTANT( OptionWarning );
	MW_REGISTER_LONG_CONSTANT( DelegateWarning );
	MW_REGISTER_LONG_CONSTANT( MissingDelegateWarning );
	MW_REGISTER_LONG_CONSTANT( CorruptImageWarning );
	MW_REGISTER_LONG_CONSTANT( FileOpenWarning );
	MW_REGISTER_LONG_CONSTANT( BlobWarning );
	MW_REGISTER_LONG_CONSTANT( StreamWarning );
	MW_REGISTER_LONG_CONSTANT( CacheWarning );
	MW_REGISTER_LONG_CONSTANT( CoderWarning );
	MW_REGISTER_LONG_CONSTANT( ModuleWarning );
	MW_REGISTER_LONG_CONSTANT( DrawWarning );
	MW_REGISTER_LONG_CONSTANT( ImageWarning );
	MW_REGISTER_LONG_CONSTANT( WandWarning );
/*	MW_REGISTER_LONG_CONSTANT( XServerWarning ); */
	MW_REGISTER_LONG_CONSTANT( MonitorWarning );
	MW_REGISTER_LONG_CONSTANT( RegistryWarning );
	MW_REGISTER_LONG_CONSTANT( ConfigureWarning );
	MW_REGISTER_LONG_CONSTANT( ErrorException );
	MW_REGISTER_LONG_CONSTANT( ResourceLimitError );
	MW_REGISTER_LONG_CONSTANT( TypeError );
	MW_REGISTER_LONG_CONSTANT( OptionError );
	MW_REGISTER_LONG_CONSTANT( DelegateError );
	MW_REGISTER_LONG_CONSTANT( MissingDelegateError );
	MW_REGISTER_LONG_CONSTANT( CorruptImageError );
	MW_REGISTER_LONG_CONSTANT( FileOpenError );
	MW_REGISTER_LONG_CONSTANT( BlobError );
	MW_REGISTER_LONG_CONSTANT( StreamError );
	MW_REGISTER_LONG_CONSTANT( CacheError );
	MW_REGISTER_LONG_CONSTANT( CoderError );
	MW_REGISTER_LONG_CONSTANT( ModuleError );
	MW_REGISTER_LONG_CONSTANT( DrawError );
	MW_REGISTER_LONG_CONSTANT( ImageError );
	MW_REGISTER_LONG_CONSTANT( WandError );
/*	MW_REGISTER_LONG_CONSTANT( XServerError ); */
	MW_REGISTER_LONG_CONSTANT( MonitorError );
	MW_REGISTER_LONG_CONSTANT( RegistryError );
	MW_REGISTER_LONG_CONSTANT( ConfigureError );
	MW_REGISTER_LONG_CONSTANT( FatalErrorException );
	MW_REGISTER_LONG_CONSTANT( ResourceLimitFatalError );
	MW_REGISTER_LONG_CONSTANT( TypeFatalError );
	MW_REGISTER_LONG_CONSTANT( OptionFatalError );
	MW_REGISTER_LONG_CONSTANT( DelegateFatalError );
	MW_REGISTER_LONG_CONSTANT( MissingDelegateFatalError );
	MW_REGISTER_LONG_CONSTANT( CorruptImageFatalError );
	MW_REGISTER_LONG_CONSTANT( FileOpenFatalError );
	MW_REGISTER_LONG_CONSTANT( BlobFatalError );
	MW_REGISTER_LONG_CONSTANT( StreamFatalError );
	MW_REGISTER_LONG_CONSTANT( CacheFatalError );
	MW_REGISTER_LONG_CONSTANT( CoderFatalError );
	MW_REGISTER_LONG_CONSTANT( ModuleFatalError );
	MW_REGISTER_LONG_CONSTANT( DrawFatalError );
	MW_REGISTER_LONG_CONSTANT( ImageFatalError );
	MW_REGISTER_LONG_CONSTANT( WandFatalError );
/*	MW_REGISTER_LONG_CONSTANT( XServerFatalError ); */
	MW_REGISTER_LONG_CONSTANT( MonitorFatalError );
	MW_REGISTER_LONG_CONSTANT( RegistryFatalError );
	MW_REGISTER_LONG_CONSTANT( ConfigureFatalError );

	/***** FillRule *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedRule );
	MW_REGISTER_LONG_CONSTANT( EvenOddRule );
	MW_REGISTER_LONG_CONSTANT( NonZeroRule );

	/***** FilterTypes *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedFilter );
	MW_REGISTER_LONG_CONSTANT( PointFilter );
	MW_REGISTER_LONG_CONSTANT( BoxFilter );
	MW_REGISTER_LONG_CONSTANT( TriangleFilter );
	MW_REGISTER_LONG_CONSTANT( HermiteFilter );
	MW_REGISTER_LONG_CONSTANT( HanningFilter );
	MW_REGISTER_LONG_CONSTANT( HammingFilter );
	MW_REGISTER_LONG_CONSTANT( BlackmanFilter );
	MW_REGISTER_LONG_CONSTANT( GaussianFilter );
	MW_REGISTER_LONG_CONSTANT( QuadraticFilter );
	MW_REGISTER_LONG_CONSTANT( CubicFilter );
	MW_REGISTER_LONG_CONSTANT( CatromFilter );
	MW_REGISTER_LONG_CONSTANT( MitchellFilter );
	MW_REGISTER_LONG_CONSTANT( LanczosFilter );
	MW_REGISTER_LONG_CONSTANT( BesselFilter );
	MW_REGISTER_LONG_CONSTANT( SincFilter );

	/***** GravityType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedGravity );
	MW_REGISTER_LONG_CONSTANT( ForgetGravity );
	MW_REGISTER_LONG_CONSTANT( NorthWestGravity );
	MW_REGISTER_LONG_CONSTANT( NorthGravity );
	MW_REGISTER_LONG_CONSTANT( NorthEastGravity );
	MW_REGISTER_LONG_CONSTANT( WestGravity );
	MW_REGISTER_LONG_CONSTANT( CenterGravity );
	MW_REGISTER_LONG_CONSTANT( EastGravity );
	MW_REGISTER_LONG_CONSTANT( SouthWestGravity );
	MW_REGISTER_LONG_CONSTANT( SouthGravity );
	MW_REGISTER_LONG_CONSTANT( SouthEastGravity );
	MW_REGISTER_LONG_CONSTANT( StaticGravity );

	/***** ImageType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedType );
	MW_REGISTER_LONG_CONSTANT( BilevelType );
	MW_REGISTER_LONG_CONSTANT( GrayscaleType );
	MW_REGISTER_LONG_CONSTANT( GrayscaleMatteType );
	MW_REGISTER_LONG_CONSTANT( PaletteType );
	MW_REGISTER_LONG_CONSTANT( PaletteMatteType );
	MW_REGISTER_LONG_CONSTANT( TrueColorType );
	MW_REGISTER_LONG_CONSTANT( TrueColorMatteType );
	MW_REGISTER_LONG_CONSTANT( ColorSeparationType );
	MW_REGISTER_LONG_CONSTANT( ColorSeparationMatteType );
	MW_REGISTER_LONG_CONSTANT( OptimizeType );

	/***** InterlaceType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedInterlace );
	MW_REGISTER_LONG_CONSTANT( NoInterlace );
	MW_REGISTER_LONG_CONSTANT( LineInterlace );
	MW_REGISTER_LONG_CONSTANT( PlaneInterlace );
	MW_REGISTER_LONG_CONSTANT( PartitionInterlace );

	/***** LineCap *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedCap );
	MW_REGISTER_LONG_CONSTANT( ButtCap );
	MW_REGISTER_LONG_CONSTANT( RoundCap );
	MW_REGISTER_LONG_CONSTANT( SquareCap );

	/***** LineJoin *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedJoin );
	MW_REGISTER_LONG_CONSTANT( MiterJoin );
	MW_REGISTER_LONG_CONSTANT( RoundJoin );
	MW_REGISTER_LONG_CONSTANT( BevelJoin );

	/***** MagickEvaluateOperator *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( AddEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( AndEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( DivideEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( LeftShiftEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( MaxEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( MinEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( MultiplyEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( OrEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( RightShiftEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( SetEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( SubtractEvaluateOperator );
	MW_REGISTER_LONG_CONSTANT( XorEvaluateOperator );

	/***** MetricType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedMetric );
	MW_REGISTER_LONG_CONSTANT( MeanAbsoluteErrorMetric );
	MW_REGISTER_LONG_CONSTANT( MeanSquaredErrorMetric );
	MW_REGISTER_LONG_CONSTANT( PeakAbsoluteErrorMetric );
	MW_REGISTER_LONG_CONSTANT( PeakSignalToNoiseRatioMetric );
	MW_REGISTER_LONG_CONSTANT( RootMeanSquaredErrorMetric );

	/***** MontageMode *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedMode );
	MW_REGISTER_LONG_CONSTANT( FrameMode );
	MW_REGISTER_LONG_CONSTANT( UnframeMode );
	MW_REGISTER_LONG_CONSTANT( ConcatenateMode );

	/***** NoiseType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedNoise );
	MW_REGISTER_LONG_CONSTANT( UniformNoise );
	MW_REGISTER_LONG_CONSTANT( GaussianNoise );
	MW_REGISTER_LONG_CONSTANT( MultiplicativeGaussianNoise );
	MW_REGISTER_LONG_CONSTANT( ImpulseNoise );
	MW_REGISTER_LONG_CONSTANT( LaplacianNoise );
	MW_REGISTER_LONG_CONSTANT( PoissonNoise );

	/***** PaintMethod *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedMethod );
	MW_REGISTER_LONG_CONSTANT( PointMethod );
	MW_REGISTER_LONG_CONSTANT( ReplaceMethod );
	MW_REGISTER_LONG_CONSTANT( FloodfillMethod );
	MW_REGISTER_LONG_CONSTANT( FillToBorderMethod );
	MW_REGISTER_LONG_CONSTANT( ResetMethod );

	/***** PreviewType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedPreview );
	MW_REGISTER_LONG_CONSTANT( RotatePreview );
	MW_REGISTER_LONG_CONSTANT( ShearPreview );
	MW_REGISTER_LONG_CONSTANT( RollPreview );
	MW_REGISTER_LONG_CONSTANT( HuePreview );
	MW_REGISTER_LONG_CONSTANT( SaturationPreview );
	MW_REGISTER_LONG_CONSTANT( BrightnessPreview );
	MW_REGISTER_LONG_CONSTANT( GammaPreview );
	MW_REGISTER_LONG_CONSTANT( SpiffPreview );
	MW_REGISTER_LONG_CONSTANT( DullPreview );
	MW_REGISTER_LONG_CONSTANT( GrayscalePreview );
	MW_REGISTER_LONG_CONSTANT( QuantizePreview );
	MW_REGISTER_LONG_CONSTANT( DespecklePreview );
	MW_REGISTER_LONG_CONSTANT( ReduceNoisePreview );
	MW_REGISTER_LONG_CONSTANT( AddNoisePreview );
	MW_REGISTER_LONG_CONSTANT( SharpenPreview );
	MW_REGISTER_LONG_CONSTANT( BlurPreview );
	MW_REGISTER_LONG_CONSTANT( ThresholdPreview );
	MW_REGISTER_LONG_CONSTANT( EdgeDetectPreview );
	MW_REGISTER_LONG_CONSTANT( SpreadPreview );
	MW_REGISTER_LONG_CONSTANT( SolarizePreview );
	MW_REGISTER_LONG_CONSTANT( ShadePreview );
	MW_REGISTER_LONG_CONSTANT( RaisePreview );
	MW_REGISTER_LONG_CONSTANT( SegmentPreview );
	MW_REGISTER_LONG_CONSTANT( SwirlPreview );
	MW_REGISTER_LONG_CONSTANT( ImplodePreview );
	MW_REGISTER_LONG_CONSTANT( WavePreview );
	MW_REGISTER_LONG_CONSTANT( OilPaintPreview );
	MW_REGISTER_LONG_CONSTANT( CharcoalDrawingPreview );
	MW_REGISTER_LONG_CONSTANT( JPEGPreview );

	/***** RenderingIntent *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedIntent );
	MW_REGISTER_LONG_CONSTANT( SaturationIntent );
	MW_REGISTER_LONG_CONSTANT( PerceptualIntent );
	MW_REGISTER_LONG_CONSTANT( AbsoluteIntent );
	MW_REGISTER_LONG_CONSTANT( RelativeIntent );

	/***** ResolutionType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedResolution );
	MW_REGISTER_LONG_CONSTANT( PixelsPerInchResolution );
	MW_REGISTER_LONG_CONSTANT( PixelsPerCentimeterResolution );

	/***** ResourceType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedResource );
	MW_REGISTER_LONG_CONSTANT( AreaResource );
	MW_REGISTER_LONG_CONSTANT( DiskResource );
	MW_REGISTER_LONG_CONSTANT( FileResource );
	MW_REGISTER_LONG_CONSTANT( MapResource );
	MW_REGISTER_LONG_CONSTANT( MemoryResource );

	/***** StatisticType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedStatistic );
	MW_REGISTER_LONG_CONSTANT( GradientStatistic );
	MW_REGISTER_LONG_CONSTANT( MaximumStatistic );
	MW_REGISTER_LONG_CONSTANT( MeanStatistic );
	MW_REGISTER_LONG_CONSTANT( MedianStatistic );
	MW_REGISTER_LONG_CONSTANT( MinimumStatistic );
	MW_REGISTER_LONG_CONSTANT( ModeStatistic );
	MW_REGISTER_LONG_CONSTANT( NonpeakStatistic );
	MW_REGISTER_LONG_CONSTANT( StandardDeviationStatistic );

	/***** StorageType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedPixel );
	MW_REGISTER_LONG_CONSTANT( CharPixel );
	MW_REGISTER_LONG_CONSTANT( ShortPixel );
	MW_REGISTER_LONG_CONSTANT( IntegerPixel );
	MW_REGISTER_LONG_CONSTANT( LongPixel );
	MW_REGISTER_LONG_CONSTANT( FloatPixel );
	MW_REGISTER_LONG_CONSTANT( DoublePixel );

	/***** StretchType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedStretch );
	MW_REGISTER_LONG_CONSTANT( NormalStretch );
	MW_REGISTER_LONG_CONSTANT( UltraCondensedStretch );
	MW_REGISTER_LONG_CONSTANT( ExtraCondensedStretch );
	MW_REGISTER_LONG_CONSTANT( CondensedStretch );
	MW_REGISTER_LONG_CONSTANT( SemiCondensedStretch );
	MW_REGISTER_LONG_CONSTANT( SemiExpandedStretch );
	MW_REGISTER_LONG_CONSTANT( ExpandedStretch );
	MW_REGISTER_LONG_CONSTANT( ExtraExpandedStretch );
	MW_REGISTER_LONG_CONSTANT( UltraExpandedStretch );
	MW_REGISTER_LONG_CONSTANT( AnyStretch );

	/***** StyleType *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedStyle );
	MW_REGISTER_LONG_CONSTANT( NormalStyle );
	MW_REGISTER_LONG_CONSTANT( ItalicStyle );
	MW_REGISTER_LONG_CONSTANT( ObliqueStyle );
	MW_REGISTER_LONG_CONSTANT( AnyStyle );

	/***** VirtualPixelMethod *****/
	MW_REGISTER_LONG_CONSTANT( UndefinedVirtualPixelMethod );
	MW_REGISTER_LONG_CONSTANT( ConstantVirtualPixelMethod );
	MW_REGISTER_LONG_CONSTANT( EdgeVirtualPixelMethod );
	MW_REGISTER_LONG_CONSTANT( MirrorVirtualPixelMethod );
	MW_REGISTER_LONG_CONSTANT( TileVirtualPixelMethod );

	return( SUCCESS );
}
/* }}} */

/* {{{ PHP_MSHUTDOWN
*/
PHP_MSHUTDOWN_FUNCTION( magickwand )
{
/*	UNREGISTER_INI_ENTRIES();
*/
	/* Destroy the MagickWand environment */
	MagickWandTerminus();

	return( SUCCESS );
}
/* }}} */


/* ZEND_MINFO_FUNCTION
 */

/*********************************** defines ***********************************/
#define PRV_STR_LEN  2056

#define PRV_PRINT_INFO( heading_str, data_str_var )  \
	snprintf( tmp_buf, PRV_STR_LEN, "%s %s", pckg_name_constant, heading_str );  \
	php_info_print_table_row( 2, tmp_buf, data_str_var );
/*******************************************************************************/

PHP_MINFO_FUNCTION( magickwand )
{
	char buffer[ PRV_STR_LEN ], tmp_buf[ PRV_STR_LEN ], *pckg_name_constant, *tmp_str_constant, **str_arr;
	unsigned long num_indices, i;

	php_info_print_table_start();

	pckg_name_constant = (char *) MagickGetPackageName();
	php_info_print_table_header( 2, "MagickWand Backend Library", pckg_name_constant );

	php_info_print_table_row( 2, "MagickWand Extension Version", MAGICKWAND_VERSION );

	PRV_PRINT_INFO( "support", "enabled" );

	tmp_str_constant = (char *) MagickGetVersion( (unsigned long *) NULL );
	PRV_PRINT_INFO( "version", tmp_str_constant );

	snprintf( buffer, PRV_STR_LEN, "%0.0f", MW_QuantumRange );
	PRV_PRINT_INFO( "QuantumRange (MaxRGB)", buffer );

/*
	tmp_str_constant = (char *) MagickGetReleaseDate();
	PRV_PRINT_INFO( "release date", tmp_str_constant );

	PRV_PRINT_INFO( "homepage", MagickHomeURL );
*/

	/************************* Display the list of available image formats. *************************/

	str_arr = (char **) MagickQueryFormats( "*", &num_indices );

	if ( str_arr != (char **) NULL && num_indices > 0 ) {

		snprintf( buffer, PRV_STR_LEN, "%s", str_arr[0] );

		for ( i = 1; i < num_indices; i++) {
			snprintf( tmp_buf, PRV_STR_LEN, "%s, %s", buffer,  str_arr[i] );
			snprintf( buffer,  PRV_STR_LEN, "%s",     tmp_buf             );
		}
		php_info_print_table_row( 2, "MagickWand supported image formats", buffer );
	}
	MW_FREE_MAGICK_MEM( char **, str_arr );

	/************************************************************************************************/

	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/*
****************************************************************************************************
*************************         Generic Wand Exception Functions         *************************
****************************************************************************************************
*/
/*  The following functions can be used to determine if ANY wand contains an error
	-- should be VERY USEFUL
*/

/* {{{ proto array WandGetException( AnyWand wand )
*/
PHP_FUNCTION( wandgetexception )
{
	MW_PRINT_DEBUG_INFO

	MW_DETERMINE_RSRC_TYPE_SET_ERROR_ARR();
}
/* }}} */

/* {{{ proto string WandGetExceptionString( AnyWand wand )
*/
PHP_FUNCTION( wandgetexceptionstring )
{
	MW_PRINT_DEBUG_INFO

	MW_DETERMINE_RSRC_TYPE_SET_ERROR_STR();
}
/* }}} */

/* {{{ proto int WandGetExceptionType( AnyWand wand )
*/
PHP_FUNCTION( wandgetexceptiontype )
{
	MW_PRINT_DEBUG_INFO

	MW_DETERMINE_RSRC_TYPE_SET_ERR_TYPE();
}
/* }}} */

/* {{{ proto bool WandHasException( AnyWand wand )
*/
PHP_FUNCTION( wandhasexception )
{
	MW_PRINT_DEBUG_INFO

	MW_DETERMINE_RSRC_TYPE_SET_ERR_BOOL();
}
/* }}} */


/*
****************************************************************************************************
*************************             Wand Identity Functions              *************************
****************************************************************************************************
*/

/* {{{ proto bool IsDrawingWand( mixed var )
*/
PHP_FUNCTION( isdrawingwand )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_VERIFY_TYPE_RET_BOOL( DrawingWand );
}
/* }}} */

/* {{{ proto bool IsMagickWand( mixed var )
*/
PHP_FUNCTION( ismagickwand )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_VERIFY_TYPE_RET_BOOL( MagickWand );
}
/* }}} */

/* {{{ proto bool IsPixelIterator( mixed var )
*/
PHP_FUNCTION( ispixeliterator )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_VERIFY_TYPE_RET_BOOL( PixelIterator );
}
/* }}} */

/* {{{ proto bool IsPixelWand( mixed var )
*/
PHP_FUNCTION( ispixelwand )
{
	MW_PRINT_DEBUG_INFO

/*  Different macro is needed due to the existence of the le_PixelIteratorPixelWand
	registered resource type, which is just a PixelWand that Zend shouldn't destroy.
*/
	MW_GET_PixelWand_VERIFY_TYPE_RET_BOOL();
}
/* }}} */


/*
****************************************************************************************************
*************************     DrawingWand Functions     *************************
****************************************************************************************************
*/

/* {{{ proto void ClearDrawingWand( DrawingWand draw_wand )
*/
PHP_FUNCTION( cleardrawingwand )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	ClearDrawingWand( draw_wand );
}
/* }}} */

/* {{{ proto DrawingWand CloneDrawingWand( DrawingWand draw_wand )
*/
PHP_FUNCTION( clonedrawingwand )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( DrawingWand, CloneDrawingWand( draw_wand ) );
}
/* }}} */

/* {{{ proto void DestroyDrawingWand( DrawingWand draw_wand )
*/
PHP_FUNCTION( destroydrawingwand )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_RSRC_DESTROY_POINTER( DrawingWand );
}
/* }}} */

/* {{{ proto void DrawAffine( DrawingWand draw_wand, float sx, float sy, float rx, float ry, float tx, float ty )
*/
PHP_FUNCTION( drawaffine )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	AffineMatrix affine_mtrx;
	zval *draw_wand_rsrc_zvl_p;
	double sx, sy, rx, ry, tx, ty;

	MW_GET_7_ARG( "rdddddd", &draw_wand_rsrc_zvl_p, &sx, &sy, &rx, &ry, &tx, &ty );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	affine_mtrx.sx = sx;
	affine_mtrx.sy = sy;

	affine_mtrx.rx = rx;
	affine_mtrx.ry = ry;

	affine_mtrx.tx = tx;
	affine_mtrx.ty = ty;

	DrawAffine( draw_wand, &affine_mtrx );
}
/* }}} */

/* {{{ proto void DrawAnnotation( DrawingWand draw_wand, float x, float y, string text )
*/
PHP_FUNCTION( drawannotation )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;
	char *txt;
	int txt_len;

	MW_GET_5_ARG( "rdds", &draw_wand_rsrc_zvl_p, &x, &y, &txt, &txt_len );

	MW_CHECK_PARAM_STR_LEN( txt_len );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawAnnotation( draw_wand, x, y, (unsigned char *) txt );
}
/* }}} */

/* {{{ proto void DrawArc( DrawingWand draw_wand, float sx, float sy, float ex, float ey, float sd, float ed )
*/
PHP_FUNCTION( drawarc )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double  sx, sy, ex, ey, sd, ed;

	MW_GET_7_ARG( "rdddddd", &draw_wand_rsrc_zvl_p, &sx, &sy, &ex, &ey, &sd, &ed );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawArc( draw_wand, sx, sy, ex, ey, sd, ed );
}
/* }}} */

/* {{{ proto void DrawBezier( DrawingWand draw_wand, array x_y_points_array )
*/
PHP_FUNCTION( drawbezier )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_AND_ARRAY_DRAW_POINTS( 6, DrawBezier );

/*	MW_GET_WAND_DRAW_POINTS( 7, DrawBezier );  */
}
/* }}} */

/* {{{ proto void DrawCircle( DrawingWand draw_wand, float ox, float oy, float px, float py )
*/
PHP_FUNCTION( drawcircle )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double ox, oy, px, py;

	MW_GET_5_ARG( "rdddd", &draw_wand_rsrc_zvl_p, &ox, &oy, &px, &py );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawCircle( draw_wand, ox, oy, px, py );
}
/* }}} */

/* {{{ proto void DrawColor( DrawingWand draw_wand, float x, float y, int paint_method )
*/
PHP_FUNCTION( drawcolor )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;
	long paint_method;

	MW_GET_4_ARG( "rddl", &draw_wand_rsrc_zvl_p, &x, &y, &paint_method );

	MW_CHECK_CONSTANT( PaintMethod, paint_method );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawColor( draw_wand, x, y, (PaintMethod) paint_method );
}
/* }}} */

/* {{{ proto void DrawComment( DrawingWand draw_wand, string comment )
*/
PHP_FUNCTION( drawcomment )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	char *comment;
	int comment_len;

	MW_GET_3_ARG( "rs", &draw_wand_rsrc_zvl_p, &comment, &comment_len );

	MW_CHECK_PARAM_STR_LEN( comment_len );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawComment( draw_wand, comment );
}
/* }}} */

/* {{{ proto bool DrawComposite( DrawingWand draw_wand, int composite_operator, float x, float y, float width, float height, MagickWand magick_wand )
*/
PHP_FUNCTION( drawcomposite )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	MagickWand *magick_wand;
	zval *draw_wand_rsrc_zvl_p, *magick_wand_rsrc_zvl_p;
	long composite_op;
	double x, y, width, height;

	MW_GET_7_ARG( "rlddddr", &draw_wand_rsrc_zvl_p, &composite_op, &x, &y, &width, &height, &magick_wand_rsrc_zvl_p );

	MW_CHECK_CONSTANT( CompositeOperator, composite_op );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( DrawComposite( draw_wand, (CompositeOperator) composite_op, x, y, width, height, magick_wand ) );
}
/* }}} */

/* {{{ proto void DrawEllipse( DrawingWand draw_wand, float ox, float oy, float rx, float ry, float start, float end )
*/
PHP_FUNCTION( drawellipse )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double ox, oy, rx, ry, start, end;

	MW_GET_7_ARG( "rdddddd", &draw_wand_rsrc_zvl_p, &ox, &oy, &rx, &ry, &start, &end );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawEllipse( draw_wand, ox, oy, rx, ry, start, end );
}
/* }}} */

/* {{{ proto string DrawGetClipPath( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetclippath )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_STRING( DrawingWand, DrawGetClipPath );
}
/* }}} */

/* {{{ proto int DrawGetClipRule( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetcliprule )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( DrawingWand, DrawGetClipRule );
}
/* }}} */

/* {{{ proto int DrawGetClipUnits( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetclipunits )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( DrawingWand, DrawGetClipUnits );
}
/* }}} */

/* {{{ proto array DrawGetException( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetexception )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_ARR( DrawingWand );
}
/* }}} */

/* {{{ proto string DrawGetExceptionString( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetexceptionstring )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_STR( DrawingWand );
}
/* }}} */

/* {{{ proto int DrawGetExceptionType( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetexceptiontype )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_TYPE( DrawingWand );
}
/* }}} */

/* {{{ proto float DrawGetFillOpacity( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetfillalpha )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( DrawingWand, DrawGetFillOpacity );
}
/* }}} */

/* {{{ proto float DrawGetFillOpacity( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetfillopacity )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( DrawingWand, DrawGetFillOpacity );
}
/* }}} */

/* {{{ proto PixelWand DrawGetFillColor( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetfillcolor )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_DO_VOID_FUNC_RET_PIXELWAND( DrawGetFillColor );
}
/* }}} */

/* {{{ proto int DrawGetFillRule( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetfillrule )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( DrawingWand, DrawGetFillRule );
}
/* }}} */

/* {{{ proto string DrawGetFont( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetfont )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_STRING( DrawingWand, DrawGetFont );
}
/* }}} */

/* {{{ proto string DrawGetFontFamily( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetfontfamily )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_STRING( DrawingWand, DrawGetFontFamily );
}
/* }}} */

/* {{{ proto float DrawGetFontSize( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetfontsize )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( DrawingWand, DrawGetFontSize );
}
/* }}} */

/* {{{ proto int DrawGetFontStretch( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetfontstretch )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( DrawingWand, DrawGetFontStretch );
}
/* }}} */

/* {{{ proto int DrawGetFontStyle( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetfontstyle )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( DrawingWand, DrawGetFontStyle );
}
/* }}} */

/* {{{ proto float DrawGetFontWeight( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetfontweight )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( DrawingWand, DrawGetFontWeight );
}
/* }}} */

/* {{{ proto int DrawGetGravity( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetgravity )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( DrawingWand, DrawGetGravity );
}
/* }}} */

/* {{{ proto float DrawGetStrokeOpacity( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetstrokealpha )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( DrawingWand, DrawGetStrokeOpacity );
}
/* }}} */

/* {{{ proto float DrawGetStrokeOpacity( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetstrokeopacity )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( DrawingWand, DrawGetStrokeOpacity );
}
/* }}} */

/* {{{ proto bool DrawGetStrokeAntialias( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetstrokeantialias )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( DrawingWand, DrawGetStrokeAntialias );
}
/* }}} */

/* {{{ proto PixelWand DrawGetStrokeColor( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetstrokecolor )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_DO_VOID_FUNC_RET_PIXELWAND( DrawGetStrokeColor );
}
/* }}} */

/* {{{ proto array DrawGetStrokeDashArray( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetstrokedasharray )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE_ARR( DrawingWand, DrawGetStrokeDashArray );
}
/* }}} */

/* {{{ proto float DrawGetStrokeDashOffset( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetstrokedashoffset )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( DrawingWand, DrawGetStrokeDashOffset );
}
/* }}} */

/* {{{ proto int DrawGetStrokeLineCap( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetstrokelinecap )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( DrawingWand, DrawGetStrokeLineCap );
}
/* }}} */

/* {{{ proto int DrawGetStrokeLineJoin( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetstrokelinejoin )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( DrawingWand, DrawGetStrokeLineJoin );
}
/* }}} */

/* {{{ proto float DrawGetStrokeMiterLimit( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetstrokemiterlimit )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( DrawingWand, DrawGetStrokeMiterLimit );
}
/* }}} */

/* {{{ proto float DrawGetStrokeWidth( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetstrokewidth )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( DrawingWand, DrawGetStrokeWidth );
}
/* }}} */

/* {{{ proto int DrawGetTextAlignment( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgettextalignment )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( DrawingWand, DrawGetTextAlignment );
}
/* }}} */

/* {{{ proto bool DrawGetTextAntialias( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgettextantialias )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( DrawingWand, DrawGetTextAntialias );
}
/* }}} */

/* {{{ proto int DrawGetTextDecoration( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgettextdecoration )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( DrawingWand, DrawGetTextDecoration );
}
/* }}} */

/* {{{ proto string DrawGetTextEncoding( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgettextencoding )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_STRING( DrawingWand, DrawGetTextEncoding );
}
/* }}} */

/* {{{ proto string DrawGetVectorGraphics( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgetvectorgraphics )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_STRING( DrawingWand, DrawGetVectorGraphics );
}
/* }}} */

/* {{{ proto PixelWand DrawGetTextUnderColor( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawgettextundercolor )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_DO_VOID_FUNC_RET_PIXELWAND( DrawGetTextUnderColor );
}
/* }}} */

/* {{{ proto void DrawLine( DrawingWand draw_wand, float sx, float sy, float ex, float ey )
*/
PHP_FUNCTION( drawline )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double sx, sy, ex, ey;

	MW_GET_5_ARG( "rdddd", &draw_wand_rsrc_zvl_p, &sx, &sy, &ex, &ey );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawLine( draw_wand, sx, sy, ex, ey );
}
/* }}} */

/* {{{ proto void DrawMatte( DrawingWand draw_wand, float x, float y, int paint_method )
*/
PHP_FUNCTION( drawmatte )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;
	long paint_method;

	MW_GET_4_ARG( "rddl", &draw_wand_rsrc_zvl_p, &x, &y, &paint_method );

	MW_CHECK_CONSTANT( PaintMethod, paint_method );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawMatte( draw_wand, x, y, (PaintMethod) paint_method );
}
/* }}} */

/* {{{ proto void DrawPathClose( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawpathclose )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathClose( draw_wand );
}
/* }}} */

/* {{{ proto void DrawPathCurveToAbsolute( DrawingWand draw_wand, float x1, float y1, float x2, float y2, float x, float y )
*/
PHP_FUNCTION( drawpathcurvetoabsolute )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x1, y1, x2, y2, x, y;

	MW_GET_7_ARG( "rdddddd", &draw_wand_rsrc_zvl_p, &x1, &y1, &x2, &y2, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathCurveToAbsolute( draw_wand, x1, y1, x2, y2, x, y );
}
/* }}} */

/* {{{ proto void DrawPathCurveToRelative( DrawingWand draw_wand, float x1, float y1, float x2, float y2, float x, float y )
*/
PHP_FUNCTION( drawpathcurvetorelative )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x1, y1, x2, y2, x, y;

	MW_GET_7_ARG( "rdddddd", &draw_wand_rsrc_zvl_p, &x1, &y1, &x2, &y2, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathCurveToRelative( draw_wand, x1, y1, x2, y2, x, y );
}
/* }}} */

/* {{{ proto void DrawPathCurveToQuadraticBezierAbsolute( DrawingWand draw_wand, float x1, float y1, float x, float y )
*/
PHP_FUNCTION( drawpathcurvetoquadraticbezierabsolute )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x1, y1, x, y;

	MW_GET_5_ARG( "rdddd", &draw_wand_rsrc_zvl_p, &x1, &y1, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathCurveToQuadraticBezierAbsolute( draw_wand, x1, y1, x, y );
}
/* }}} */

/* {{{ proto void DrawPathCurveToQuadraticBezierRelative( DrawingWand draw_wand, float x1, float y1, float x, float y )
*/
PHP_FUNCTION( drawpathcurvetoquadraticbezierrelative )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x1, y1, x, y;

	MW_GET_5_ARG( "rdddd", &draw_wand_rsrc_zvl_p, &x1, &y1, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathCurveToQuadraticBezierRelative( draw_wand, x1, y1, x, y );
}
/* }}} */

/* {{{ proto void DrawPathCurveToQuadraticBezierSmoothAbsolute( DrawingWand draw_wand, float x, float y )
*/
PHP_FUNCTION( drawpathcurvetoquadraticbeziersmoothabsolute )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &draw_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathCurveToQuadraticBezierSmoothAbsolute( draw_wand, x, y );
}
/* }}} */

/* {{{ proto void DrawPathCurveToQuadraticBezierSmoothRelative( DrawingWand draw_wand, float x, float y )
*/
PHP_FUNCTION( drawpathcurvetoquadraticbeziersmoothrelative )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &draw_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathCurveToQuadraticBezierSmoothRelative( draw_wand, x, y );
}
/* }}} */

/* {{{ proto void DrawPathCurveToSmoothAbsolute( DrawingWand draw_wand, float x2, float y2, float x, float y )
*/
PHP_FUNCTION( drawpathcurvetosmoothabsolute )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x2, y2, x, y;

	MW_GET_5_ARG( "rdddd", &draw_wand_rsrc_zvl_p, &x2, &y2, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathCurveToSmoothAbsolute( draw_wand, x2, y2, x, y );
}
/* }}} */

/* {{{ proto void DrawPathCurveToSmoothRelative( DrawingWand draw_wand, float x2, float y2, float x, float y )
*/
PHP_FUNCTION( drawpathcurvetosmoothrelative )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x2, y2, x, y;

	MW_GET_5_ARG( "rdddd", &draw_wand_rsrc_zvl_p, &x2, &y2, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathCurveToSmoothRelative( draw_wand, x2, y2, x, y );
}
/* }}} */

/* {{{ proto void DrawPathEllipticArcAbsolute( DrawingWand draw_wand, float rx, float ry, float x_axis_rotation, bool large_arc_flag, bool sweep_flag, float x, float y )
*/
PHP_FUNCTION( drawpathellipticarcabsolute )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double rx, ry, x_axis_rotation, x, y;
	zend_bool do_large_arc, do_sweep;

	MW_GET_8_ARG( "rdddbbdd", &draw_wand_rsrc_zvl_p, &rx, &ry, &x_axis_rotation, &do_large_arc, &do_sweep, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathEllipticArcAbsolute(
		draw_wand,
		rx, ry,
		x_axis_rotation,
		MW_MK_MGCK_BOOL(do_large_arc),
		MW_MK_MGCK_BOOL(do_sweep),
		x, y
	);
}
/* }}} */

/* {{{ proto void DrawPathEllipticArcRelative( DrawingWand draw_wand, float rx, float ry, float x_axis_rotation, bool large_arc_flag, bool sweep_flag, float x, float y )
*/
PHP_FUNCTION( drawpathellipticarcrelative )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double rx, ry, x_axis_rotation, x, y;
	zend_bool do_large_arc, do_sweep;

	MW_GET_8_ARG( "rdddbbdd", &draw_wand_rsrc_zvl_p, &rx, &ry, &x_axis_rotation, &do_large_arc, &do_sweep, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathEllipticArcRelative(
		draw_wand,
		rx, ry,
		x_axis_rotation,
		MW_MK_MGCK_BOOL(do_large_arc),
		MW_MK_MGCK_BOOL(do_sweep),
		x, y
	);
}
/* }}} */

/* {{{ proto void DrawPathFinish( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawpathfinish )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathFinish( draw_wand );
}
/* }}} */

/* {{{ proto void DrawPathLineToAbsolute( DrawingWand draw_wand, float x, float y )
*/
PHP_FUNCTION( drawpathlinetoabsolute )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &draw_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathLineToAbsolute( draw_wand, x, y );
}
/* }}} */

/* {{{ proto void DrawPathLineToRelative( DrawingWand draw_wand, float x, float y )
*/
PHP_FUNCTION( drawpathlinetorelative )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &draw_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathLineToRelative( draw_wand, x, y );
}
/* }}} */

/* {{{ proto void DrawPathLineToHorizontalAbsolute( DrawingWand draw_wand, float x )
*/
PHP_FUNCTION( drawpathlinetohorizontalabsolute )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_DOUBLE( DrawPathLineToHorizontalAbsolute );
}
/* }}} */

/* {{{ proto void DrawPathLineToHorizontalRelative( DrawingWand draw_wand, float x )
*/
PHP_FUNCTION( drawpathlinetohorizontalrelative )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_DOUBLE( DrawPathLineToHorizontalRelative );
}
/* }}} */

/* {{{ proto void DrawPathLineToVerticalAbsolute( DrawingWand draw_wand, float y )
*/
PHP_FUNCTION( drawpathlinetoverticalabsolute )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_DOUBLE( DrawPathLineToVerticalAbsolute );
}
/* }}} */

/* {{{ proto void DrawPathLineToVerticalRelative( DrawingWand draw_wand, float y )
*/
PHP_FUNCTION( drawpathlinetoverticalrelative )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_DOUBLE( DrawPathLineToVerticalRelative );
}
/* }}} */

/* {{{ proto void DrawPathMoveToAbsolute( DrawingWand draw_wand, float x, float y )
*/
PHP_FUNCTION( drawpathmovetoabsolute )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &draw_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathMoveToAbsolute( draw_wand, x, y );
}
/* }}} */

/* {{{ proto void DrawPathMoveToRelative( DrawingWand draw_wand, float x, float y )
*/
PHP_FUNCTION( drawpathmovetorelative )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &draw_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathMoveToRelative( draw_wand, x, y );
}
/* }}} */

/* {{{ proto void DrawPathStart( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawpathstart )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPathStart( draw_wand );
}
/* }}} */

/* {{{ proto void DrawPoint( DrawingWand draw_wand, float x, float y )
*/
PHP_FUNCTION( drawpoint )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &draw_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPoint( draw_wand, x, y );
}
/* }}} */

/* {{{ proto void DrawPolygon( DrawingWand draw_wand, array x_y_points_array )
*/
PHP_FUNCTION( drawpolygon )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_AND_ARRAY_DRAW_POINTS( 6, DrawPolygon );

/*	MW_GET_WAND_DRAW_POINTS( 7, DrawPolygon );  */
}
/* }}} */

/* {{{ proto void DrawPolyline( DrawingWand draw_wand, array x_y_points_array )
*/
PHP_FUNCTION( drawpolyline )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_AND_ARRAY_DRAW_POINTS( 4, DrawPolyline );

/*	MW_GET_WAND_DRAW_POINTS( 5, DrawPolyline );  */
}
/* }}} */

/* {{{ proto void DrawPopClipPath( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawpopclippath )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPopClipPath( draw_wand );
}
/* }}} */

/* {{{ proto void DrawPopDefs( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawpopdefs )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPopDefs( draw_wand );
}
/* }}} */

/* {{{ proto bool DrawPopPattern( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawpoppattern )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( DrawingWand, DrawPopPattern );
}
/* }}} */

/* {{{ proto void DrawPushClipPath( DrawingWand draw_wand, string clip_path_id )
*/
PHP_FUNCTION( drawpushclippath )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	char *clip_path_id;
	int clip_path_id_len;

	MW_GET_3_ARG( "rs", &draw_wand_rsrc_zvl_p, &clip_path_id, &clip_path_id_len );

	MW_CHECK_PARAM_STR_LEN( clip_path_id_len );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPushClipPath( draw_wand, clip_path_id );
}
/* }}} */

/* {{{ proto void DrawPushDefs( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawpushdefs )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawPushDefs( draw_wand );
}
/* }}} */

/* {{{ proto bool DrawPushPattern( DrawingWand draw_wand, string pattern_id, float x, float y, float width, float height )
*/
PHP_FUNCTION( drawpushpattern )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	char *pttrn_id;
	int pttrn_id_len;
	double x, y, width, height;

	MW_GET_7_ARG( "rsdddd", &draw_wand_rsrc_zvl_p, &pttrn_id, &pttrn_id_len, &x, &y, &width, &height );

	MW_CHECK_PARAM_STR_LEN( pttrn_id_len );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( DrawPushPattern( draw_wand, pttrn_id, x, y, width, height ) );
}
/* }}} */

/* {{{ proto void DrawRectangle( DrawingWand draw_wand, float x1, float y1, float x2, float y2 )
*/
PHP_FUNCTION( drawrectangle )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x1, y1, x2, y2;

	MW_GET_5_ARG( "rdddd", &draw_wand_rsrc_zvl_p, &x1, &y1, &x2, &y2 );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawRectangle( draw_wand, x1, y1, x2, y2 );
}
/* }}} */

/* {{{ proto bool DrawRender( DrawingWand draw_wand )
*/
PHP_FUNCTION( drawrender )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( DrawingWand, DrawRender );
}
/* }}} */

/* {{{ proto void DrawRotate( DrawingWand draw_wand, float degrees )
*/
PHP_FUNCTION( drawrotate )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_DOUBLE( DrawRotate );
}
/* }}} */

/* {{{ proto void DrawRoundRectangle( DrawingWand draw_wand, float x1, float y1, float x2, float y2, float rx, float ry )
*/
PHP_FUNCTION( drawroundrectangle )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x1, y1, x2, y2, rx, ry;

	MW_GET_7_ARG( "rdddddd", &draw_wand_rsrc_zvl_p, &x1, &y1, &x2, &y2, &rx, &ry );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawRoundRectangle( draw_wand, x1, y1, x2, y2, rx, ry );
}
/* }}} */

/* {{{ proto void DrawScale( DrawingWand draw_wand, float x, float y )
*/
PHP_FUNCTION( drawscale )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &draw_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawScale( draw_wand, x, y );
}
/* }}} */

/* {{{ proto bool DrawSetClipPath( DrawingWand draw_wand, string clip_path )
*/
PHP_FUNCTION( drawsetclippath )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_AND_STRING_RETVAL_FUNC_BOOL( DrawingWand, DrawSetClipPath );
}
/* }}} */

/* {{{ proto void DrawSetClipRule( DrawingWand draw_wand, int fill_rule )
*/
PHP_FUNCTION( drawsetcliprule )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_ENUM( FillRule, DrawSetClipRule );
}
/* }}} */

/* {{{ proto void DrawSetClipUnits( DrawingWand draw_wand, int clip_path_units )
*/
PHP_FUNCTION( drawsetclipunits )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_ENUM( ClipPathUnits, DrawSetClipUnits );
}
/* }}} */

/* {{{ proto void DrawSetFillOpacity( DrawingWand draw_wand, float fill_opacity )
*/
PHP_FUNCTION( drawsetfillalpha )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( DrawingWand, DrawSetFillOpacity );
}
/* }}} */

/* {{{ proto void DrawSetFillOpacity( DrawingWand draw_wand, float fill_opacity )
*/
PHP_FUNCTION( drawsetfillopacity )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( DrawingWand, DrawSetFillOpacity );
}
/* }}} */

/* {{{ proto void DrawSetFillColor( DrawingWand draw_wand, mixed fill_pixel_wand )
*/
PHP_FUNCTION( drawsetfillcolor )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	PixelWand *fill_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 2),
							zvl_pp_args_arr,
							DrawingWand, draw_wand,
							"a DrawingWand resource, a fill color PixelWand resource "  \
							"(or ImageMagick color string)" );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, fill_pixel_wand, is_script_pixel_wand );

	DrawSetFillColor( draw_wand, fill_pixel_wand );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		fill_pixel_wand = DestroyPixelWand( fill_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool DrawSetFillPatternURL( DrawingWand draw_wand, string fill_url )
*/
PHP_FUNCTION( drawsetfillpatternurl )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_AND_STRING_RETVAL_FUNC_BOOL( DrawingWand, DrawSetFillPatternURL );
}
/* }}} */

/* {{{ proto void DrawSetFillRule( DrawingWand draw_wand, int fill_rule )
*/
PHP_FUNCTION( drawsetfillrule )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_ENUM( FillRule, DrawSetFillRule );
}
/* }}} */

/* {{{ proto bool DrawSetFont( DrawingWand draw_wand, string font_file )
*/
PHP_FUNCTION( drawsetfont )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	char *font_filename, real_filename[MAXPATHLEN];
	int font_filename_len;

	MW_GET_3_ARG( "rs", &draw_wand_rsrc_zvl_p, &font_filename, &font_filename_len );

	MW_CHECK_PARAM_STR_LEN( font_filename_len );

	real_filename[0] = '\0';
	expand_filepath(font_filename, real_filename TSRMLS_CC);

	if ( real_filename[0] == '\0' || MW_FILE_FAILS_INI_TESTS( real_filename ) ) {
		zend_error( MW_E_ERROR, "%s(): PHP cannot read %s; possible php.ini restrictions",
								get_active_function_name(TSRMLS_C), real_filename );
		RETVAL_FALSE;
	}

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( DrawSetFont( draw_wand, real_filename ) );
}
/* }}} */

/* {{{ proto bool DrawSetFontFamily( DrawingWand draw_wand, string font_family )
*/
PHP_FUNCTION( drawsetfontfamily )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_AND_STRING_RETVAL_FUNC_BOOL( DrawingWand, DrawSetFontFamily );
}
/* }}} */

/* {{{ proto void DrawSetFontSize( DrawingWand draw_wand, float pointsize )
*/
PHP_FUNCTION( drawsetfontsize )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_DOUBLE( DrawSetFontSize );
}
/* }}} */

/* {{{ proto void DrawSetFontStretch( DrawingWand draw_wand, int stretch_type )
*/
PHP_FUNCTION( drawsetfontstretch )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_ENUM( StretchType, DrawSetFontStretch );
}
/* }}} */

/* {{{ proto void DrawSetFontStyle( DrawingWand draw_wand, int style_type )
*/
PHP_FUNCTION( drawsetfontstyle )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_ENUM( StyleType, DrawSetFontStyle );
}
/* }}} */

/* {{{ proto void DrawSetFontWeight( DrawingWand draw_wand, float font_weight )
*/
PHP_FUNCTION( drawsetfontweight )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	long font_weight;

	MW_GET_2_ARG( "rl", &draw_wand_rsrc_zvl_p, &font_weight );

	MW_CHECK_LONG_VAL_RANGE( font_weight, 100, 900 );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawSetFontWeight( draw_wand, (unsigned long) font_weight );
}
/* }}} */

/* {{{ proto void DrawSetGravity( DrawingWand draw_wand, int gravity_type )
*/
PHP_FUNCTION( drawsetgravity )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_ENUM( GravityType, DrawSetGravity );
}
/* }}} */

/* {{{ proto void DrawSetStrokeOpacity( DrawingWand draw_wand, float stroke_opacity )
*/
PHP_FUNCTION( drawsetstrokealpha )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( DrawingWand, DrawSetStrokeOpacity );
}
/* }}} */

/* {{{ proto void DrawSetStrokeOpacity( DrawingWand draw_wand, float stroke_opacity )
*/
PHP_FUNCTION( drawsetstrokeopacity )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( DrawingWand, DrawSetStrokeOpacity );
}
/* }}} */

/* {{{ proto void DrawSetStrokeAntialias( DrawingWand draw_wand [, bool stroke_antialias] )
*/
PHP_FUNCTION( drawsetstrokeantialias )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	zend_bool anti_alias = TRUE;

	MW_GET_2_ARG( "r|b", &draw_wand_rsrc_zvl_p, &anti_alias );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawSetStrokeAntialias( draw_wand, MW_MK_MGCK_BOOL(anti_alias) );
}
/* }}} */

/* {{{ proto void DrawSetStrokeColor( DrawingWand draw_wand, mixed strokecolor_pixel_wand )
*/
PHP_FUNCTION( drawsetstrokecolor )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	PixelWand *strokecolor_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 2),
							zvl_pp_args_arr,
							DrawingWand, draw_wand,
							"a DrawingWand resource, a strokecolor color PixelWand resource "  \
							"(or ImageMagick color string)" );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, strokecolor_pixel_wand, is_script_pixel_wand );

	DrawSetStrokeColor( draw_wand, strokecolor_pixel_wand );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		strokecolor_pixel_wand = DestroyPixelWand( strokecolor_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool DrawSetStrokeDashArray( DrawingWand draw_wand [, array dash_array] )
*/
PHP_FUNCTION( drawsetstrokedasharray )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p, *zvl_arr = (zval *) NULL, **zvl_pp_element;
	unsigned long num_elements, i = 0;
	double *dash_arr;
	HashPosition pos;

	MW_GET_2_ARG( "r|a!", &draw_wand_rsrc_zvl_p, &zvl_arr );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	if ( zvl_arr == (zval *) NULL || Z_TYPE_P( zvl_arr ) == IS_NULL ) {
		MW_BOOL_FUNC_RETVAL_BOOL( DrawSetStrokeDashArray( draw_wand, 0, (double *) NULL ) );
	}
	else {
		num_elements = (unsigned long) zend_hash_num_elements( Z_ARRVAL_P( zvl_arr ) );

		if ( num_elements == 0 ) {
			MW_BOOL_FUNC_RETVAL_BOOL( DrawSetStrokeDashArray( draw_wand, 0, (double *) NULL ) );
		}
		else {
			MW_ARR_ECALLOC( double, dash_arr, num_elements );

			MW_ITERATE_OVER_PHP_ARRAY( pos, zvl_arr, zvl_pp_element ) {
				convert_to_double_ex( zvl_pp_element );
				dash_arr[ i++ ] = Z_DVAL_PP( zvl_pp_element );
			}
			MW_BOOL_FUNC_RETVAL_BOOL( DrawSetStrokeDashArray( draw_wand, num_elements, dash_arr ) );
			efree( dash_arr );
		}
	}
}
/* }}} */

/* {{{ proto void DrawSetStrokeDashOffset( DrawingWand draw_wand, float dash_offset )
*/
PHP_FUNCTION( drawsetstrokedashoffset )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_DOUBLE( DrawSetStrokeDashOffset );
}
/* }}} */

/* {{{ proto void DrawSetStrokeLineCap( DrawingWand draw_wand, int line_cap )
*/
PHP_FUNCTION( drawsetstrokelinecap )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_ENUM( LineCap, DrawSetStrokeLineCap );
}
/* }}} */

/* {{{ proto void DrawSetStrokeLineJoin( DrawingWand draw_wand, int line_join )
*/
PHP_FUNCTION( drawsetstrokelinejoin )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_ENUM( LineJoin, DrawSetStrokeLineJoin );
}
/* }}} */

/* {{{ proto void DrawSetStrokeMiterLimit( DrawingWand draw_wand, float miterlimit )
*/
PHP_FUNCTION( drawsetstrokemiterlimit )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double miterlimit;

	MW_GET_2_ARG( "rd", &draw_wand_rsrc_zvl_p, &miterlimit );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawSetStrokeMiterLimit( draw_wand, (unsigned long) miterlimit );
}
/* }}} */

/* {{{ proto bool DrawSetStrokePatternURL( DrawingWand draw_wand, string stroke_url )
*/
PHP_FUNCTION( drawsetstrokepatternurl )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_AND_STRING_RETVAL_FUNC_BOOL( DrawingWand, DrawSetStrokePatternURL );
}
/* }}} */

/* {{{ proto void DrawSetStrokeWidth( DrawingWand draw_wand, float stroke_width )
*/
PHP_FUNCTION( drawsetstrokewidth )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_DOUBLE( DrawSetStrokeWidth );
}
/* }}} */

/* {{{ proto void DrawSetTextAlignment( DrawingWand draw_wand, int align_type )
*/
PHP_FUNCTION( drawsettextalignment )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_ENUM( AlignType, DrawSetTextAlignment );
}
/* }}} */

/* {{{ proto void DrawSetTextAntialias( DrawingWand draw_wand [, bool text_antialias] )
*/
PHP_FUNCTION( drawsettextantialias )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	zend_bool anti_alias = TRUE;

	MW_GET_2_ARG( "r|b", &draw_wand_rsrc_zvl_p, &anti_alias );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawSetTextAntialias( draw_wand, MW_MK_MGCK_BOOL(anti_alias) );
}
/* }}} */

/* {{{ proto void DrawSetTextDecoration( DrawingWand draw_wand, int decoration_type )
*/
PHP_FUNCTION( drawsettextdecoration )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_ENUM( DecorationType, DrawSetTextDecoration );
}
/* }}} */

/* {{{ proto void DrawSetTextEncoding( DrawingWand draw_wand, string encoding )
*/
PHP_FUNCTION( drawsettextencoding )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	char *enc_str;
	int enc_str_len;

	MW_GET_3_ARG( "rs", &draw_wand_rsrc_zvl_p, &enc_str, &enc_str_len );

	MW_CHECK_PARAM_STR_LEN( enc_str_len );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawSetTextEncoding( draw_wand, enc_str );
}
/* }}} */

/* {{{ proto void DrawSetTextUnderColor( DrawingWand draw_wand, mixed undercolor_pixel_wand )
*/
PHP_FUNCTION( drawsettextundercolor )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	PixelWand *undercolor_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 2),
							zvl_pp_args_arr,
							DrawingWand, draw_wand,
							"a DrawingWand resource and a undercolor PixelWand resource "  \
							"(or ImageMagick color string)" );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, undercolor_pixel_wand, is_script_pixel_wand );

	DrawSetTextUnderColor( draw_wand, undercolor_pixel_wand );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		undercolor_pixel_wand = DestroyPixelWand( undercolor_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool DrawSetVectorGraphics( DrawingWand draw_wand, string vector_graphics )
*/
PHP_FUNCTION( drawsetvectorgraphics )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	char *str;
	int str_len;

	MW_GET_3_ARG( "r|s", &draw_wand_rsrc_zvl_p, &str, &str_len );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );
	MW_BOOL_FUNC_RETVAL_BOOL( DrawSetVectorGraphics( draw_wand, ( str_len > 0 ? str : "" ) ) );
}
/* }}} */

/* {{{ proto void DrawSkewX( DrawingWand draw_wand, float degrees )
*/
PHP_FUNCTION( drawskewx )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_DOUBLE( DrawSkewX );
}
/* }}} */

/* {{{ proto void DrawSkewY( DrawingWand draw_wand, float degrees )
*/
PHP_FUNCTION( drawskewy )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_DRAWINGWAND_SET_DOUBLE( DrawSkewY );
}
/* }}} */

/* {{{ proto void DrawTranslate( DrawingWand draw_wand, float x, float y )
*/
PHP_FUNCTION( drawtranslate )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &draw_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawTranslate( draw_wand, x, y );
}
/* }}} */

/* {{{ proto void DrawSetViewbox( DrawingWand draw_wand, float x1, float y1, float x2, float y2 )
*/
PHP_FUNCTION( drawsetviewbox )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;
	double x1, y1, x2, y2;

	MW_GET_5_ARG( "rdddd", &draw_wand_rsrc_zvl_p, &x1, &y1, &x2, &y2 );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	DrawSetViewbox(
		draw_wand,
		(unsigned long) x1, (unsigned long) y1,
		(unsigned long) x2, (unsigned long) y2
	);
}
/* }}} */

/* {{{ proto DrawingWand NewDrawingWand( void )
*/
PHP_FUNCTION( newdrawingwand )
{
	MW_PRINT_DEBUG_INFO

	MW_SET_RET_WAND_RSRC_FROM_FUNC( DrawingWand, NewDrawingWand() );
}
/* }}} */

/* {{{ proto void PopDrawingWand( DrawingWand draw_wand )
*/
PHP_FUNCTION( popdrawingwand )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	PopDrawingWand( draw_wand );
}
/* }}} */

/* {{{ proto void PushDrawingWand( DrawingWand draw_wand )
*/
PHP_FUNCTION( pushdrawingwand )
{
	MW_PRINT_DEBUG_INFO

	DrawingWand *draw_wand;
	zval *draw_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	PushDrawingWand( draw_wand );
}
/* }}} */


/*
****************************************************************************************************
*************************     MagickWand Functions     *************************
****************************************************************************************************
*/

/* {{{ proto void ClearMagickWand( MagickWand magick_wand )
*/
PHP_FUNCTION( clearmagickwand )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	ClearMagickWand( magick_wand );
}
/* }}} */

/* {{{ proto MagickWand NewMagickWand( void )
*/
PHP_FUNCTION( newmagickwand )
{
	MW_PRINT_DEBUG_INFO

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, NewMagickWand() );
}
/* }}} */

/* {{{ proto MagickWand CloneMagickWand( MagickWand magick_wand )
*/
PHP_FUNCTION( clonemagickwand )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, CloneMagickWand( magick_wand ) );
}
/* }}} */

/* {{{ proto void DestroyMagickWand( MagickWand magick_wand )
*/
PHP_FUNCTION( destroymagickwand )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_RSRC_DESTROY_POINTER( MagickWand );
}
/* }}} */

/* {{{ proto bool MagickAdaptiveThresholdImage( MagickWand magick_wand, float width, float height, int offset )
*/
PHP_FUNCTION( magickadaptivethresholdimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double width, height;
	long offset;

	MW_GET_4_ARG( "rddl", &magick_wand_rsrc_zvl_p, &width, &height, &offset );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickAdaptiveThresholdImage( magick_wand, (unsigned long) width, (unsigned long) height, offset ) );
}
/* }}} */

/* {{{ proto bool MagickAddImage( MagickWand magick_wand, MagickWand add_wand )
*/
PHP_FUNCTION( magickaddimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand, *get_wnd, *add_wnd;
	zval *magick_wand_rsrc_zvl_p, *add_wnd_rsrc_zvl_p;

	MW_GET_2_ARG( "rr", &magick_wand_rsrc_zvl_p, &add_wnd_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, add_wnd, &add_wnd_rsrc_zvl_p );

	get_wnd = (MagickWand *) MagickGetImage( add_wnd );

	if ( get_wnd == (MagickWand *) NULL ) {

		MW_API_FUNC_FAIL_CHECK_WAND_ERROR(	add_wnd, MagickWand,
											"unable to retrieve the current active image of the 2nd MagickWand "  \
											"resource argument; unable to perform the copy operation"
		);

		return;
	}

	MW_BOOL_FUNC_RETVAL_BOOL( MagickAddImage( magick_wand, get_wnd ) );

	get_wnd = DestroyMagickWand( get_wnd );
}
/* }}} */

/* {{{ proto bool MagickAddImages( MagickWand magick_wand, MagickWand add_wand )
*/
PHP_FUNCTION( magickaddimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand, *add_wnd;
	zval *magick_wand_rsrc_zvl_p, *add_wnd_rsrc_zvl_p;

	MW_GET_2_ARG( "rr", &magick_wand_rsrc_zvl_p, &add_wnd_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, add_wnd, &add_wnd_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickAddImage( magick_wand, add_wnd ) );
}
/* }}} */

/* {{{ proto bool MagickAddNoiseImage( MagickWand magick_wand, int noise_type )
*/
PHP_FUNCTION( magickaddnoiseimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( NoiseType, MagickAddNoiseImage );
}
/* }}} */

/* {{{ proto bool MagickAffineTransformImage( MagickWand magick_wand, DrawingWand draw_wand )
*/
PHP_FUNCTION( magickaffinetransformimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	DrawingWand *draw_wand;
	zval *magick_wand_rsrc_zvl_p, *draw_wand_rsrc_zvl_p;

	MW_GET_2_ARG( "rr", &magick_wand_rsrc_zvl_p, &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickAffineTransformImage( magick_wand, draw_wand ) );
}
/* }}} */

/* {{{ proto bool MagickAnnotateImage( MagickWand magick_wand, DrawingWand draw_wand, float x, float y, float angle, string text )
*/
PHP_FUNCTION( magickannotateimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	DrawingWand *draw_wand;
	zval *magick_wand_rsrc_zvl_p, *draw_wand_rsrc_zvl_p;
	double x, y, angle;
	char *txt;
	int txt_len;

	MW_GET_7_ARG( "rrddds", &magick_wand_rsrc_zvl_p, &draw_wand_rsrc_zvl_p, &x, &y, &angle, &txt, &txt_len );

	MW_CHECK_PARAM_STR_LEN( txt_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickAnnotateImage( magick_wand, draw_wand, x, y, angle, txt ) );
}
/* }}} */

/* {{{ proto MagickWand MagickAppendImages( MagickWand magick_wand [, bool stack_vertical] )
*/
PHP_FUNCTION( magickappendimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	zend_bool stack_vertical = FALSE;

	MW_GET_2_ARG( "r|b", &magick_wand_rsrc_zvl_p, &stack_vertical );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickAppendImages( magick_wand, MW_MK_MGCK_BOOL(stack_vertical) ) );
}
/* }}} */

/* {{{ proto MagickWand MagickAverageImages( MagickWand magick_wand )
*/
PHP_FUNCTION( magickaverageimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickEvaluateImages( magick_wand, MeanEvaluateOperator ) );
}
/* }}} */

/* {{{ proto bool MagickBlackThresholdImage( MagickWand magick_wand, mixed threshold_pixel_wand )
*/
PHP_FUNCTION( magickblackthresholdimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *threshold_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 2),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, a threshold color PixelWand resource "  \
							"(or ImageMagick color string)" );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, threshold_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickBlackThresholdImage( magick_wand, threshold_pixel_wand ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		threshold_pixel_wand = DestroyPixelWand( threshold_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickBlurImage( MagickWand magick_wand, float radius, float sigma [, int channel_type] )
*/
PHP_FUNCTION( magickblurimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double radius, sigma;
	long channel = -1;

	MW_GET_4_ARG( "rdd|l", &magick_wand_rsrc_zvl_p, &radius, &sigma, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickBlurImage( magick_wand, radius, sigma ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickBlurImageChannel( magick_wand, (ChannelType) channel, radius, sigma ) );
	}
}
/* }}} */

/* {{{ proto bool MagickBorderImage( MagickWand magick_wand, mixed bordercolor, float width, float height )
*/
PHP_FUNCTION( magickborderimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *bordercolor_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;
	double width, height;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 4),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, a bordercolor PixelWand resource "  \
							"(or ImageMagick color string), and the desired border width and height" );

	convert_to_double_ex( zvl_pp_args_arr[2] );
	width =    Z_DVAL_PP( zvl_pp_args_arr[2] );

	convert_to_double_ex( zvl_pp_args_arr[3] );
	height =   Z_DVAL_PP( zvl_pp_args_arr[3] );

	if ( width < 1 && height < 1 ) {
		MW_SPIT_FATAL_ERR( "cannot create an image border smaller than 1 pixel in width and height" );
		efree( zvl_pp_args_arr );
		return;
	}

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, bordercolor_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickBorderImage( magick_wand, bordercolor_pixel_wand, (unsigned long) width, (unsigned long) height ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		bordercolor_pixel_wand = DestroyPixelWand( bordercolor_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickCharcoalImage( MagickWand magick_wand, float radius, float sigma )
*/
PHP_FUNCTION( magickcharcoalimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double radius, sigma;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &radius, &sigma );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickCharcoalImage( magick_wand, radius, sigma ) );
}
/* }}} */

/* {{{ proto bool MagickChopImage( MagickWand magick_wand, int width, int height, int x, int y )
*/
PHP_FUNCTION( magickchopimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double width, height;
	long x, y;

	MW_GET_5_ARG( "rddll", &magick_wand_rsrc_zvl_p, &width, &height, &x, &y );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickChopImage( magick_wand, (unsigned long) width, (unsigned long) height, x, y ) );
}
/* }}} */

/* {{{ proto bool MagickClipImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickclipimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickClipImage );
}
/* }}} */

/* {{{ proto bool MagickClipImagePath( MagickWand magick_wand, string pathname, bool inside )
*/
PHP_FUNCTION( magickclippathimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *path_name;
	int path_name_len;
	zend_bool clip_inside;

	MW_GET_4_ARG( "rsb", &magick_wand_rsrc_zvl_p, &path_name, &path_name_len, &clip_inside );

	MW_CHECK_PARAM_STR_LEN( path_name_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickClipImagePath( magick_wand, path_name, MW_MK_MGCK_BOOL(clip_inside) ) );
}
/* }}} */

/* {{{ proto MagickWand MagickCoalesceImages( MagickWand magick_wand )
*/
PHP_FUNCTION( magickcoalesceimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickCoalesceImages( magick_wand ) );
}
/* }}} */

/* {{{ proto bool MagickColorFloodfillImage( MagickWand magick_wand, mixed fillcolor_pixel_wand, float fuzz, mixed bordercolor_pixel_wand, int x, int y )
*/
PHP_FUNCTION( magickcolorfloodfillimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *fillcolor_pixel_wand, *bordercolor_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand, is_script_pixel_wand_2;
	double fuzz;
	long x, y;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 6),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, a fill color PixelWand resource (or ImageMagick "  \
							"color string), a fuzz value, NULL or a bordercolor PixelWand resource (or "  \
							"ImageMagick color string), and the x and y ordinates of the starting pixel" );

	convert_to_double_ex( zvl_pp_args_arr[2] );
	fuzz =     Z_DVAL_PP( zvl_pp_args_arr[2] );

	convert_to_long_ex( zvl_pp_args_arr[4] );
	x =      Z_LVAL_PP( zvl_pp_args_arr[4] );

	convert_to_long_ex( zvl_pp_args_arr[5] );
	y =      Z_LVAL_PP( zvl_pp_args_arr[5] );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, fillcolor_pixel_wand, is_script_pixel_wand );

	if ( Z_TYPE_PP( zvl_pp_args_arr[3] ) == IS_RESOURCE ) {

		if (  (   MW_FETCH_RSRC( PixelWand, bordercolor_pixel_wand, zvl_pp_args_arr[3] ) == MagickFalse
			   && MW_FETCH_RSRC( PixelIteratorPixelWand, bordercolor_pixel_wand, zvl_pp_args_arr[3] ) == MagickFalse )
			|| IsPixelWand( bordercolor_pixel_wand ) == MagickFalse )
		{
			MW_SPIT_FATAL_ERR( "invalid resource type as argument #4; a PixelWand resource is required" );
			efree( zvl_pp_args_arr );
			if ( !is_script_pixel_wand ) {
				fillcolor_pixel_wand = DestroyPixelWand( fillcolor_pixel_wand );
			}
			return;
		}
		is_script_pixel_wand_2 = 1;
	}
	else {
		if ( Z_TYPE_PP( zvl_pp_args_arr[3] ) == IS_NULL ) {
			is_script_pixel_wand_2 = 1;
			bordercolor_pixel_wand = (PixelWand *) NULL;
		}
		else {
			is_script_pixel_wand_2 = 0;

			bordercolor_pixel_wand = (PixelWand *) NewPixelWand();

			if ( bordercolor_pixel_wand == (PixelWand *) NULL ) {
				MW_SPIT_FATAL_ERR( "unable to create necessary PixelWand" );
				efree( zvl_pp_args_arr );
				if ( !is_script_pixel_wand ) {
					fillcolor_pixel_wand = DestroyPixelWand( fillcolor_pixel_wand );
				}
				return;
			}
			convert_to_string_ex( zvl_pp_args_arr[3] );

			if ( Z_STRLEN_PP( zvl_pp_args_arr[3] ) > 0 ) {

				if ( PixelSetColor( bordercolor_pixel_wand, Z_STRVAL_PP( zvl_pp_args_arr[3] ) ) == MagickFalse ) {

					MW_API_FUNC_FAIL_CHECK_WAND_ERROR(	bordercolor_pixel_wand, PixelWand,
														"could not set PixelWand to desired fill color"
					);

					bordercolor_pixel_wand = DestroyPixelWand( bordercolor_pixel_wand );
					efree( zvl_pp_args_arr );

					if ( !is_script_pixel_wand ) {
						fillcolor_pixel_wand = DestroyPixelWand( fillcolor_pixel_wand );
					}

					return;
				}
			}
		}
	}

	MW_BOOL_FUNC_RETVAL_BOOL( MagickFloodfillPaintImage( magick_wand, AllChannels, fillcolor_pixel_wand, fuzz, bordercolor_pixel_wand, x, y, MagickFalse ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		fillcolor_pixel_wand = DestroyPixelWand( fillcolor_pixel_wand );
	}
	if ( !is_script_pixel_wand_2 ) {
		bordercolor_pixel_wand = DestroyPixelWand( bordercolor_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickColorizeImage( MagickWand magick_wand, mixed colorize_pixel_wand, mixed opacity_pixel_wand )
*/
PHP_FUNCTION( magickcolorizeimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *colorize_pixel_wand, *opacity_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand, is_script_pixel_wand_2;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 3),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource and a tint color PixelWand resource (or ImageMagick color string)" );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, colorize_pixel_wand, is_script_pixel_wand );

	if ( Z_TYPE_PP( zvl_pp_args_arr[2] ) == IS_RESOURCE ) {

		if (  (   MW_FETCH_RSRC( PixelWand, opacity_pixel_wand, zvl_pp_args_arr[2] ) == MagickFalse
			   && MW_FETCH_RSRC( PixelIteratorPixelWand, opacity_pixel_wand, zvl_pp_args_arr[2] ) == MagickFalse )
			|| IsPixelWand( opacity_pixel_wand ) == MagickFalse )
		{
			MW_SPIT_FATAL_ERR( "invalid resource type as argument #3; a PixelWand resource is required" );
			efree( zvl_pp_args_arr );
			if ( !is_script_pixel_wand ) {
				colorize_pixel_wand = DestroyPixelWand( colorize_pixel_wand );
			}
			return;
		}
		is_script_pixel_wand_2 = 1;
	}
	else {
		is_script_pixel_wand_2 = 0;

		opacity_pixel_wand = (PixelWand *) NewPixelWand();

		if ( opacity_pixel_wand == (PixelWand *) NULL ) {
			MW_SPIT_FATAL_ERR( "unable to create necessary PixelWand" );
			efree( zvl_pp_args_arr );
			if ( !is_script_pixel_wand ) {
				colorize_pixel_wand = DestroyPixelWand( colorize_pixel_wand );
			}
			return;
		}
		convert_to_string_ex( zvl_pp_args_arr[2] );

		if ( Z_STRLEN_PP( zvl_pp_args_arr[2] ) > 0 ) {

			if ( PixelSetColor( opacity_pixel_wand, Z_STRVAL_PP( zvl_pp_args_arr[2] ) ) == MagickFalse ) {

				MW_API_FUNC_FAIL_CHECK_WAND_ERROR(	opacity_pixel_wand, PixelWand,
													"could not set PixelWand to desired fill color"
				);

				opacity_pixel_wand = DestroyPixelWand( opacity_pixel_wand );
				efree( zvl_pp_args_arr );

				if ( !is_script_pixel_wand ) {
					colorize_pixel_wand = DestroyPixelWand( colorize_pixel_wand );
				}

				return;
			}
		}
	}

	MW_BOOL_FUNC_RETVAL_BOOL( MagickColorizeImage( magick_wand, colorize_pixel_wand, opacity_pixel_wand ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		colorize_pixel_wand = DestroyPixelWand( colorize_pixel_wand );
	}
	if ( !is_script_pixel_wand_2 ) {
		opacity_pixel_wand = DestroyPixelWand( opacity_pixel_wand );
	}
}
/* }}} */

/* {{{ proto MagickWand MagickCombineImages( MagickWand magick_wand, int channel_type )
*/
PHP_FUNCTION( magickcombineimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long channel;

	MW_GET_2_ARG( "rl", &magick_wand_rsrc_zvl_p, &channel );

	MW_CHECK_CONSTANT( ChannelType, channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickCombineImages( magick_wand, (ChannelType) channel ) );
}
/* }}} */

/* {{{ proto bool MagickCommentImage( MagickWand magick_wand, string comment )
*/
PHP_FUNCTION( magickcommentimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_AND_STRING_RETVAL_FUNC_BOOL( MagickWand, MagickCommentImage );
}
/* }}} */

/* {{{ proto array MagickCompareImages( MagickWand magick_wand, MagickWand reference_wnd, int metric_type [, int channel_type] )
*/
PHP_FUNCTION( magickcompareimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand, *ref_wnd, *comp_wnd;
	zval *magick_wand_rsrc_zvl_p, *ref_wnd_rsrc_zvl_p;
	long metric, channel = -1;
	double distortion;
	int comp_wnd_rsrc_id;

	MW_GET_4_ARG( "rrl|l", &magick_wand_rsrc_zvl_p, &ref_wnd_rsrc_zvl_p, &metric, &channel );

	MW_CHECK_CONSTANT( MetricType, metric );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, ref_wnd, &ref_wnd_rsrc_zvl_p );

	if ( channel == -1 ) {
		comp_wnd = (MagickWand *) MagickCompareImages( magick_wand, ref_wnd, (MetricType) metric, &distortion );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		comp_wnd = (MagickWand *) MagickCompareImageChannels( magick_wand, ref_wnd, (ChannelType) channel, (MetricType) metric, &distortion );
	}

	if ( comp_wnd == (MagickWand *) NULL ) {
		RETURN_FALSE;
	}

	if ( MW_ZEND_REGISTER_RESOURCE( MagickWand, comp_wnd, (zval *) NULL, &comp_wnd_rsrc_id ) == MagickFalse ) {
		comp_wnd = DestroyMagickWand( comp_wnd );
		RETURN_FALSE;
	}

	array_init( return_value );

	MW_SET_2_RET_ARR_VALS( add_next_index_resource( return_value, comp_wnd_rsrc_id ),
						   add_next_index_double( return_value, distortion ) );
}
/* }}} */

/* {{{ proto bool MagickCompositeImage( MagickWand magick_wand, MagickWand composite_wnd, int composite_operator, int x, int y )
*/
PHP_FUNCTION( magickcompositeimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand, *composite_wnd;
	zval *magick_wand_rsrc_zvl_p, *composite_wnd_rsrc_zvl_p;
	long compose, x, y;

	MW_GET_5_ARG( "rrlll", &magick_wand_rsrc_zvl_p, &composite_wnd_rsrc_zvl_p, &compose, &x, &y );

	MW_CHECK_CONSTANT( CompositeOperator, compose );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, composite_wnd, &composite_wnd_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickCompositeImage( magick_wand, composite_wnd, (CompositeOperator) compose, x, y ) );
}
/* }}} */

/* {{{ proto bool MagickConstituteImage( MagickWand magick_wand, float columns, float rows, string map, int storage_type, array pixel_array )
*/
PHP_FUNCTION( magickconstituteimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p, *zvl_arr, **zvl_pp_element;
	double columns, rows;
	char *map, *c;
	int map_len, i, pixel_array_length;
	long php_storage;
	StorageType storage;
	HashPosition pos;

	MW_GET_7_ARG( "rddsla",
				  &magick_wand_rsrc_zvl_p,
				  &columns, &rows,
				  &map, &map_len,
				  &php_storage,
				  &zvl_arr );

	MW_CHECK_PARAM_STR_LEN( map_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	for ( i = 0; i < map_len; i++ ) {
		c = &(map[i]);

		if ( !( *c == 'A' || *c == 'a' ||
				*c == 'B' || *c == 'b' ||
				*c == 'C' || *c == 'c' ||
				*c == 'G' || *c == 'g' ||
				*c == 'I' || *c == 'i' ||
				*c == 'K' || *c == 'k' ||
				*c == 'M' || *c == 'm' ||
				*c == 'O' || *c == 'o' ||
				*c == 'P' || *c == 'p' ||
				*c == 'R' || *c == 'r' ||
				*c == 'Y' || *c == 'y' ) ) {
			MW_SPIT_FATAL_ERR( "map parameter string can only contain the letters A B C G I K M O P R Y; see docs for details" );
			return;
		}
	}

	pixel_array_length = zend_hash_num_elements( Z_ARRVAL_P( zvl_arr ) );

	if ( pixel_array_length == 0 ) {
		MW_SPIT_FATAL_ERR( "pixel array parameter was empty" );
		return;
	}

	i = (int) ( columns * rows * map_len );

	if ( pixel_array_length != i ) {
		zend_error( MW_E_ERROR, "%s(): actual size of pixel array (%d) does not match expected size (%u)",
								get_active_function_name( TSRMLS_C ), pixel_array_length, i );
		return;
	}

	storage = (StorageType) php_storage;

	i = 0;

#define PRV_SET_PIXELS_ARR_RET_BOOL_1( type )  \
{   type *pixel_array;  \
	MW_ARR_ECALLOC( type, pixel_array, pixel_array_length );  \
	MW_ITERATE_OVER_PHP_ARRAY( pos, zvl_arr, zvl_pp_element )  \
	{  \
		convert_to_double_ex( zvl_pp_element );  \
		pixel_array[ i++ ] = (type) Z_DVAL_PP( zvl_pp_element );  \
	}  \
	MW_BOOL_FUNC_RETVAL_BOOL(  \
		MagickConstituteImage( magick_wand,  \
							   (unsigned long) columns, (unsigned long) rows,  \
							   map,  \
							   storage,  \
							   (void *) pixel_array ) );  \
	efree( pixel_array );  \
}

	switch ( storage ) {
		 case CharPixel		: PRV_SET_PIXELS_ARR_RET_BOOL_1( unsigned char  );  break;
		 case ShortPixel	: PRV_SET_PIXELS_ARR_RET_BOOL_1( unsigned short );  break;
		 case IntegerPixel	: PRV_SET_PIXELS_ARR_RET_BOOL_1( unsigned int   );  break;
		 case LongPixel		: PRV_SET_PIXELS_ARR_RET_BOOL_1( unsigned long  );  break;
		 case FloatPixel	: PRV_SET_PIXELS_ARR_RET_BOOL_1( float          );  break;
		 case DoublePixel	: PRV_SET_PIXELS_ARR_RET_BOOL_1( double         );  break;

		 default : MW_SPIT_FATAL_ERR( "Invalid StorageType argument supplied to function" );
				   return;
	}
}
/* }}} */

/* {{{ proto bool MagickContrastImage( MagickWand magick_wand, bool sharpen )
*/
PHP_FUNCTION( magickcontrastimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	zend_bool sharpen = FALSE;

	MW_GET_2_ARG( "r|b", &magick_wand_rsrc_zvl_p, &sharpen );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickContrastImage( magick_wand, MW_MK_MGCK_BOOL(sharpen) ) );
}
/* }}} */

/* {{{ proto bool MagickConvolveImage( MagickWand magick_wand, array kernel_array [, int channel_type] )
*/
PHP_FUNCTION( magickconvolveimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p, *zvl_arr, **zvl_pp_element;
	long channel = -1;
	unsigned long order, i = 0;
	double num_elements, *kernel_arr;
	HashPosition pos;

	MW_GET_3_ARG( "ra|l", &magick_wand_rsrc_zvl_p, &zvl_arr, &channel );

	num_elements = (double) zend_hash_num_elements( Z_ARRVAL_P( zvl_arr ) );

	if ( num_elements < 1 ) {
		MW_SPIT_FATAL_ERR( "the array parameter was empty" );
		return;
	}

	order = (unsigned long) sqrt( num_elements );

	if ( pow( (double) order, 2 ) != num_elements ) {
		MW_SPIT_FATAL_ERR( "array parameter length was not square; array must contain a square number amount of doubles" );
		return;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_ARR_ECALLOC( double, kernel_arr, num_elements );

	MW_ITERATE_OVER_PHP_ARRAY( pos, zvl_arr, zvl_pp_element ) {
		convert_to_double_ex( zvl_pp_element );

		kernel_arr[i++] = Z_DVAL_PP( zvl_pp_element );
	}

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickConvolveImage( magick_wand, order, kernel_arr ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickConvolveImageChannel( magick_wand, (ChannelType) channel, order, kernel_arr ) );
	}

	efree( kernel_arr );
}
/* }}} */

/* {{{ proto bool MagickCropImage( MagickWand magick_wand, int width, int height, int x, int y )
*/
PHP_FUNCTION( magickcropimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double width, height;
	long x, y;

	MW_GET_5_ARG( "rddll", &magick_wand_rsrc_zvl_p, &width, &height, &x, &y );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickCropImage( magick_wand, (unsigned long) width, (unsigned long) height, x, y ) );
}
/* }}} */

/* {{{ proto bool MagickCycleColormapImage( MagickWand magick_wand, int num_positions )
*/
PHP_FUNCTION( magickcyclecolormapimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long displace;

	MW_GET_2_ARG( "rl", &magick_wand_rsrc_zvl_p, &displace );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickCycleColormapImage( magick_wand, displace ) );
}
/* }}} */

/* {{{ proto MagickWand MagickDeconstructImages( MagickWand magick_wand )
*/
PHP_FUNCTION( magickdeconstructimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickDeconstructImages( magick_wand ) );
}
/* }}} */

/* {{{ proto string MagickIdentifyImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickdescribeimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_STRING( MagickWand, MagickIdentifyImage );
}
/* }}} */

/* {{{ proto string MagickIdentifyImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickidentifyimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_STRING( MagickWand, MagickIdentifyImage );
}
/* }}} */

/* {{{ proto bool MagickDespeckleImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickdespeckleimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickDespeckleImage );
}
/* }}} */

/* {{{ proto bool MagickDisplayImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickdisplayimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long img_idx;
	char *format, *mime_type, *orig_filename = (char *) NULL, *orig_img_format = (char *) NULL, header[100];
	int filename_valid;
	unsigned char *blob;
	size_t blob_len = 0;
	MagickBooleanType img_had_format;
	sapi_header_line ctr = {0};

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );
	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	/* Saves the current active image index, as well as performs a shortcut check for any exceptions,
	   as they will occur if magick_wand contains no images
	*/
	img_idx = (long) MagickGetIteratorIndex( magick_wand );
	if ( MagickGetExceptionType(magick_wand) != UndefinedException  ) {
		RETURN_FALSE;
	}
	MagickClearException( magick_wand );

	MW_ENSURE_IMAGE_HAS_FORMAT( magick_wand, img_idx, img_had_format, orig_img_format, "MagickDisplayImage" );

	format = (char *) (MagickGetImageFormat ( magick_wand ));

	mime_type = (char *) MagickToMime( format );

	if ( mime_type == (char *) NULL || *mime_type == '\0' ) {
		zend_error( MW_E_ERROR, "%s(): a mime-type for the specified image format (%s) could not be found",
								get_active_function_name(TSRMLS_C), format );
		MW_FREE_MAGICK_MEM( char *, orig_img_format );
		MW_FREE_MAGICK_MEM( char *, mime_type );
		MW_FREE_MAGICK_MEM( char *, format );
		return;
	}
	else {
		MW_FREE_MAGICK_MEM( char *, format );

		orig_filename = (char *) MagickGetImageFilename( magick_wand );
		filename_valid = !(orig_filename == (char *) NULL || *orig_filename == '\0');

		if ( filename_valid ) {
			MagickSetImageFilename( magick_wand, (char *) NULL );
		}

		blob = (unsigned char *) MagickGetImageBlob( magick_wand, &blob_len );

		if ( blob == (unsigned char *) NULL || *blob == '\0' ) {

			if ( MagickGetExceptionType(magick_wand) == UndefinedException ) {
				zend_error( MW_E_ERROR, "%s(): an unknown error occurred; the image BLOB to be output was empty",
										get_active_function_name(TSRMLS_C) );
			}
			else {
				char *mw_err_str;
				ExceptionType mw_severity;

				mw_err_str = (char *) MagickGetException( magick_wand, &mw_severity );
				if ( mw_err_str == (char *) NULL || *mw_err_str == '\0' ) {
					zend_error( MW_E_ERROR, "%s(): an unknown exception occurred", get_active_function_name(TSRMLS_C) );
				}
				else {
					zend_error( MW_E_ERROR, "%s(): a MagickWand exception occurred: %s",
											get_active_function_name(TSRMLS_C), mw_err_str );
				}
				MW_FREE_MAGICK_MEM( char *, mw_err_str );
			}

			MW_FREE_MAGICK_MEM( char *, mime_type );
			MW_FREE_MAGICK_MEM( unsigned char *, blob );

			MW_FREE_MAGICK_MEM( char *, orig_img_format );
			MW_FREE_MAGICK_MEM( char *, orig_filename );

			return;
		}
		else {
			snprintf( header, 100, "Content-type: %s", mime_type );
			ctr.line = header;
			ctr.line_len = strlen(header);
			ctr.response_code = 200;

			sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);

			php_write( blob, blob_len TSRMLS_CC);
			RETVAL_TRUE;
		}
		MW_FREE_MAGICK_MEM( char *, mime_type );
		MW_FREE_MAGICK_MEM( unsigned char *, blob );

		if ( filename_valid ) {
			MagickSetImageFilename( magick_wand, orig_filename );
		}
		MW_FREE_MAGICK_MEM( char *, orig_filename );

		/* Check if current image had a format before function started */
		if ( img_had_format == MagickFalse ) {

			MW_DEBUG_NOTICE_EX( "Attempting to reset image #%d's image format", img_idx );

			/* If not, it must have been changed to the MagickWand's format, so set it back (probably to nothing) */
			if ( MagickSetImageFormat( magick_wand, orig_img_format ) == MagickFalse ) {

				MW_DEBUG_NOTICE_EX(	"FAILURE: could not reset image #%d's image format", img_idx );

				/* C API could not reset set the current image's format back to its original state:
				   output error, with reason
				*/
				MW_API_FUNC_FAIL_CHECK_WAND_ERROR_EX_1(	magick_wand, MagickWand,
														"unable to set the image at MagickWand index %ld back to "  \
														"its original image format",
														img_idx
				);
			}
		}
		MW_FREE_MAGICK_MEM( char *, orig_img_format );
	}
}
/* }}} */

/* {{{ proto bool MagickDisplayImages( MagickWand magick_wand )
*/
PHP_FUNCTION( magickdisplayimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long img_idx;
	char *format, *mime_type, *orig_filename = (char *) NULL, header[100];
	int filename_valid;
	unsigned char *blob;
	size_t blob_len = 0;
	sapi_header_line ctr = {0};

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );
	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	/* Saves the current active image index, as well as performs a shortcut check for any exceptions,
	   as they will occur if magick_wand contains no images
	*/
	img_idx = (long) MagickGetIteratorIndex( magick_wand );
	if ( MagickGetExceptionType(magick_wand) != UndefinedException  ) {
		RETURN_FALSE;
	}
	MagickClearException( magick_wand );

	format = (char *) MagickGetFormat( magick_wand );

	if ( format == (char *) NULL || *format == '\0' || *format == '*' ) {
		MW_SPIT_FATAL_ERR(	"the MagickWand resource sent to this function did not have an image format set "
							"(via MagickSetFormat()); the MagickWand's image format must be set in order "
							"for this MagickDisplayImages() to continue" );
		MW_FREE_MAGICK_MEM( char *, format );
		return;
	}

	mime_type = (char *) MagickToMime( format );

	if ( mime_type == (char *) NULL || *mime_type == '\0' ) {
		zend_error( MW_E_ERROR, "%s(): a mime-type for the specified image format (%s) could not be found",
								get_active_function_name(TSRMLS_C), format );
		MW_FREE_MAGICK_MEM( char *, mime_type );
		MW_FREE_MAGICK_MEM( char *, format );
		return;
	}
	else {
		MW_FREE_MAGICK_MEM( char *, format );

		orig_filename = (char *) MagickGetFilename( magick_wand );
		filename_valid = !(orig_filename == (char *) NULL || *orig_filename == '\0');

		if ( filename_valid ) {
			MagickSetFilename( magick_wand, (char *) NULL );
		}

		blob = (unsigned char *) MagickGetImagesBlob( magick_wand, &blob_len );

		if ( blob == (unsigned char *) NULL || *blob == '\0' ) {

			if ( MagickGetExceptionType(magick_wand) == UndefinedException ) {
				zend_error( MW_E_ERROR, "%s(): an unknown error occurred; the image BLOB to be output was empty",
										get_active_function_name(TSRMLS_C) );
			}
			else {
				char *mw_err_str;
				ExceptionType mw_severity;

				mw_err_str = (char *) MagickGetException( magick_wand, &mw_severity );
				if ( mw_err_str == (char *) NULL || *mw_err_str == '\0' ) {
					zend_error( MW_E_ERROR, "%s(): an unknown exception occurred", get_active_function_name(TSRMLS_C) );
				}
				else {
					zend_error( MW_E_ERROR, "%s(): a MagickWand exception occurred: %s",
											get_active_function_name(TSRMLS_C), mw_err_str );
				}
				MW_FREE_MAGICK_MEM( char *, mw_err_str );
			}

			MW_FREE_MAGICK_MEM( unsigned char *, blob );
			MW_FREE_MAGICK_MEM( char *, mime_type );

			MW_FREE_MAGICK_MEM( char *, orig_filename );

			return;
		}
		else {
			snprintf( header, 100, "Content-type: %s", mime_type );
			ctr.line = header;
			ctr.line_len = strlen(header);
			ctr.response_code = 200;

			sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);

			php_write( blob, blob_len TSRMLS_CC);
			RETVAL_TRUE;
		}
		MW_FREE_MAGICK_MEM( char *, mime_type );
		MW_FREE_MAGICK_MEM( unsigned char *, blob );

		if ( filename_valid ) {
			MagickSetFilename( magick_wand, orig_filename );
		}
		MW_FREE_MAGICK_MEM( char *, orig_filename );
	}
}
/* }}} */

/* {{{ proto bool MagickDrawImage( MagickWand magick_wand, DrawingWand draw_wand )
*/
PHP_FUNCTION( magickdrawimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	DrawingWand *draw_wand;
	zval *magick_wand_rsrc_zvl_p, *draw_wand_rsrc_zvl_p;

	MW_GET_2_ARG( "rr", &magick_wand_rsrc_zvl_p, &draw_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickDrawImage( magick_wand, draw_wand ) );
}
/* }}} */

/* {{{ proto bool MagickEchoImageBlob( MagickWand magick_wand )
*/
PHP_FUNCTION( magickechoimageblob )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long img_idx;
	char *orig_filename = (char *) NULL, *orig_img_format = (char *) NULL;
	int filename_valid;
	unsigned char *blob;
	size_t blob_len = 0;
	MagickBooleanType img_had_format;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );
	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	/* Saves the current active image index, as well as performs a shortcut check for any exceptions,
	   as they will occur if magick_wand contains no images
	*/
	img_idx = (long) MagickGetIteratorIndex( magick_wand );
	if ( MagickGetExceptionType(magick_wand) != UndefinedException  ) {
		RETURN_FALSE;
	}
	MagickClearException( magick_wand );

	MW_ENSURE_IMAGE_HAS_FORMAT( magick_wand, img_idx, img_had_format, orig_img_format, "MagickEchoImageBlob" );

	orig_filename = (char *) MagickGetImageFilename( magick_wand );
	filename_valid = !(orig_filename == (char *) NULL || *orig_filename == '\0');

	if ( filename_valid ) {
		MagickSetImageFilename( magick_wand, (char *) NULL );
	}

	blob = (unsigned char *) MagickGetImageBlob( magick_wand, &blob_len );

	if ( blob == (unsigned char *) NULL || *blob == '\0' ) {

		if ( MagickGetExceptionType(magick_wand) == UndefinedException ) {
			zend_error( MW_E_ERROR, "%s(): an unknown error occurred; the image BLOB to be output was empty",
									get_active_function_name(TSRMLS_C) );
		}
		else {
			char *mw_err_str;
			ExceptionType mw_severity;

			mw_err_str = (char *) MagickGetException( magick_wand, &mw_severity );
			if ( mw_err_str == (char *) NULL || *mw_err_str == '\0' ) {
				zend_error( MW_E_ERROR, "%s(): an unknown exception occurred", get_active_function_name(TSRMLS_C) );
			}
			else {
				zend_error( MW_E_ERROR, "%s(): a MagickWand exception occurred: %s",
										get_active_function_name(TSRMLS_C), mw_err_str );
			}
			MW_FREE_MAGICK_MEM( char *, mw_err_str );
		}

		MW_FREE_MAGICK_MEM( unsigned char *, blob );

		MW_FREE_MAGICK_MEM( char *, orig_img_format );
		MW_FREE_MAGICK_MEM( char *, orig_filename );

		return;
	}
	else {
		php_write( blob, blob_len TSRMLS_CC);
		RETVAL_TRUE;
	}
	MW_FREE_MAGICK_MEM( unsigned char *, blob );

	if ( filename_valid ) {
		MagickSetImageFilename( magick_wand, orig_filename );
	}
	MW_FREE_MAGICK_MEM( char *, orig_filename );

	/* Check if current image had a format before function started */
	if ( img_had_format == MagickFalse ) {

		MW_DEBUG_NOTICE_EX( "Attempting to reset image #%d's image format", img_idx );

		/* If not, it must have been changed to the MagickWand's format, so set it back (probably to nothing) */
		if ( MagickSetImageFormat( magick_wand, orig_img_format ) == MagickFalse ) {

			MW_DEBUG_NOTICE_EX(	"FAILURE: could not reset image #%d's image format", img_idx );

			/* C API could not reset set the current image's format back to its original state:
			   output error, with reason
			*/
			MW_API_FUNC_FAIL_CHECK_WAND_ERROR_EX_1(	magick_wand, MagickWand,
													"unable to set the image at MagickWand index %ld back to "  \
													"its original image format",
													img_idx
			);
		}
	}
	MW_FREE_MAGICK_MEM( char *, orig_img_format );
}
/* }}} */

/* {{{ proto bool MagickEchoImagesBlob( MagickWand magick_wand )
*/
PHP_FUNCTION( magickechoimagesblob )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long img_idx;
	char *orig_filename = (char *) NULL;
	int filename_valid;
	unsigned char *blob;
	size_t blob_len = 0;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );
	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	/* Saves the current active image index, as well as performs a shortcut check for any exceptions,
	   as they will occur if magick_wand contains no images
	*/
	img_idx = (long) MagickGetIteratorIndex( magick_wand );
	if ( MagickGetExceptionType(magick_wand) != UndefinedException  ) {
		RETURN_FALSE;
	}
	MagickClearException( magick_wand );

	MW_CHECK_FOR_WAND_FORMAT( magick_wand, "MagickEchoImagesBlob" );

	orig_filename = (char *) MagickGetFilename( magick_wand );
	filename_valid = !(orig_filename == (char *) NULL || *orig_filename == '\0');

	if ( filename_valid ) {
		MagickSetFilename( magick_wand, (char *) NULL );
	}

	blob = (unsigned char *) MagickGetImagesBlob( magick_wand, &blob_len );

	if ( blob == (unsigned char *) NULL || *blob == '\0' ) {

		if ( MagickGetExceptionType(magick_wand) == UndefinedException ) {
			zend_error( MW_E_ERROR, "%s(): an unknown error occurred; the image BLOB to be output was empty",
									get_active_function_name(TSRMLS_C) );
		}
		else {
			char *mw_err_str;
			ExceptionType mw_severity;

			mw_err_str = (char *) MagickGetException( magick_wand, &mw_severity );
			if ( mw_err_str == (char *) NULL || *mw_err_str == '\0' ) {
				zend_error( MW_E_ERROR, "%s(): an unknown exception occurred", get_active_function_name(TSRMLS_C) );
			}
			else {
				zend_error( MW_E_ERROR, "%s(): a MagickWand exception occurred: %s",
										get_active_function_name(TSRMLS_C), mw_err_str );
			}
			MW_FREE_MAGICK_MEM( char *, mw_err_str );
		}

		MW_FREE_MAGICK_MEM( unsigned char *, blob );
		MW_FREE_MAGICK_MEM( char *, orig_filename );

		return;
	}
	else {
		php_write( blob, blob_len TSRMLS_CC);
		RETVAL_TRUE;
	}
	MW_FREE_MAGICK_MEM( unsigned char *, blob );

	if ( filename_valid ) {
		MagickSetFilename( magick_wand, orig_filename );
	}
	MW_FREE_MAGICK_MEM( char *, orig_filename );
}
/* }}} */

/* {{{ proto bool MagickEdgeImage( MagickWand magick_wand, float radius )
*/
PHP_FUNCTION( magickedgeimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_DOUBLE_RET_BOOL( MagickEdgeImage );
}
/* }}} */

/* {{{ proto bool MagickEmbossImage( MagickWand magick_wand, float radius, float sigma )
*/
PHP_FUNCTION( magickembossimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double radius, sigma;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &radius, &sigma );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickEmbossImage( magick_wand, radius, sigma ) );
}
/* }}} */

/* {{{ proto bool MagickEnhanceImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickenhanceimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickEnhanceImage );
}
/* }}} */

/* {{{ proto bool MagickEqualizeImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickequalizeimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickEqualizeImage );
}
/* }}} */

/* {{{ proto bool MagickEvaluateImage( MagickWand magick_wand, int evaluate_op, float constant [, int channel_type] )
*/
PHP_FUNCTION( magickevaluateimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long eval_op, channel = -1;
	double constant;

	MW_GET_4_ARG( "rld|l", &magick_wand_rsrc_zvl_p, &eval_op, &constant, &channel );

	MW_CHECK_CONSTANT( MagickEvaluateOperator, eval_op );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickEvaluateImage( magick_wand, (MagickEvaluateOperator) eval_op, constant ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickEvaluateImageChannel( magick_wand, (ChannelType) channel, (MagickEvaluateOperator) eval_op, constant ) );
	}
}
/* }}} */

/* {{{ proto MagickWand MagickFlattenImages( MagickWand magick_wand )
*/
PHP_FUNCTION( magickflattenimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickMergeImageLayers( magick_wand, FlattenLayer ) );
}
/* }}} */

/* {{{ proto bool MagickFlipImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickflipimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickFlipImage );
}
/* }}} */

/* {{{ proto bool MagickFlopImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickflopimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickFlopImage );
}
/* }}} */

/* {{{ proto bool MagickFrameImage( MagickWand magick_wand, mixed matte_pixel_wand, float width, float height, int inner_bevel, int outer_bevel )
*/
PHP_FUNCTION( magickframeimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *matte_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;
	double width, height;
	long inner_bevel, outer_bevel;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 6),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, a matte color PixelWand resource (or "  \
							"ImageMagick color string), the desired frame width and height, "  \
							"and the desired inner and outer bevel sizes" );

	convert_to_double_ex( zvl_pp_args_arr[2] );
	width =    Z_DVAL_PP( zvl_pp_args_arr[2] );

	convert_to_double_ex( zvl_pp_args_arr[3] );
	height =   Z_DVAL_PP( zvl_pp_args_arr[3] );
/*
	if ( width < 1 && height < 1 ) {
		MW_SPIT_FATAL_ERR( "cannot create a frame smaller than 1 pixel in width and height" );
		efree( zvl_pp_args_arr );
		return;
	}
*/
	convert_to_long_ex(      zvl_pp_args_arr[4] );
	inner_bevel = Z_LVAL_PP( zvl_pp_args_arr[4] );

	convert_to_long_ex(      zvl_pp_args_arr[5] );
	outer_bevel = Z_LVAL_PP( zvl_pp_args_arr[5] );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, matte_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL(
		MagickFrameImage( magick_wand, matte_pixel_wand, (unsigned long) width, (unsigned long) height, inner_bevel, outer_bevel )
	);

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		matte_pixel_wand = DestroyPixelWand( matte_pixel_wand );
	}
}
/* }}} */

/* {{{ proto MagickWand MagickFxImage( MagickWand magick_wand, string expression [, int channel_type] )
*/
PHP_FUNCTION( magickfximage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *expression;
	int expression_len;
	long channel = -1;

	MW_GET_4_ARG( "rs|l", &magick_wand_rsrc_zvl_p, &expression, &expression_len, &channel );

	MW_CHECK_PARAM_STR_LEN( expression_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickFxImage( magick_wand, expression ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickFxImageChannel( magick_wand, (ChannelType) channel, expression ) );
	}
}
/* }}} */

/* {{{ proto bool MagickGammaImage( MagickWand magick_wand, float gamma [, int channel_type] )
*/
PHP_FUNCTION( magickgammaimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long channel = -1;
	double gamma;

	MW_GET_3_ARG( "rd|l", &magick_wand_rsrc_zvl_p, &gamma, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickGammaImage( magick_wand, gamma ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickGammaImageChannel( magick_wand, (ChannelType) channel, gamma ) );
	}
}
/* }}} */

/* {{{ proto bool MagickGaussianBlurImage( MagickWand magick_wand, float radius, float sigma [, int channel_type] )
*/
PHP_FUNCTION( magickgaussianblurimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double radius, sigma;
	long channel = -1;

	MW_GET_4_ARG( "rdd|l", &magick_wand_rsrc_zvl_p, &radius, &sigma, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickGaussianBlurImage( magick_wand, radius, sigma ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickGaussianBlurImageChannel( magick_wand, (ChannelType) channel, radius, sigma ) );
	}
}
/* }}} */

/* {{{ proto int MagickGetCompression( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetcompression )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetCompression );
}
/* }}} */

/* {{{ proto float MagickGetCompressionQuality( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetcompressionquality )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetCompressionQuality );
}
/* }}} */

/* {{{ proto string MagickGetCopyright( void )
*/
PHP_FUNCTION( magickgetcopyright )
{
	MW_PRINT_DEBUG_INFO

	char *copyright;

/* No freeing of copyright after this call, because string returned is a constant string, ie "..."
*/
	copyright = (char *) MagickGetCopyright();

	RETURN_STRING( copyright, 1 );
}
/* }}} */

/* {{{ proto array MagickGetException( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetexception )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_ARR( MagickWand );
}
/* }}} */

/* {{{ proto string MagickGetExceptionString( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetexceptionstring )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_STR( MagickWand );
}
/* }}} */

/* {{{ proto int MagickGetExceptionType( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetexceptiontype )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_TYPE( MagickWand );
}
/* }}} */

/* {{{ proto string MagickGetFilename( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetfilename )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_STRING( MagickWand,  MagickGetFilename );
}
/* }}} */

/* {{{ proto string MagickGetFormat( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetformat )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *img_format;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	img_format = (char *) MagickGetFormat( magick_wand );

	if ( img_format == (char *) NULL || *img_format == '\0' || *img_format == '*' ) {

		if ( MagickGetExceptionType(magick_wand) == UndefinedException ) {
			RETVAL_MW_EMPTY_STRING();
		}
		else {
			RETVAL_FALSE;
		}
	}
	else {
		RETVAL_STRING( img_format, 1 );
	}

	MW_FREE_MAGICK_MEM( char *, img_format );
}
/* }}} */

/* {{{ proto string MagickGetHomeURL( void )
*/
PHP_FUNCTION( magickgethomeurl )
{
	MW_PRINT_DEBUG_INFO

	char *ret_str;

	ret_str = (char *) MagickGetHomeURL();

	if ( ret_str == (char *) NULL || *ret_str == '\0' ) {
		RETURN_MW_EMPTY_STRING();
	}

	RETVAL_STRING( ret_str, 1 );

	MW_FREE_MAGICK_MEM( char *, ret_str );
}
/* }}} */

/* {{{ proto MagickWand MagickGetImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickGetImage( magick_wand ) );
}
/* }}} */

/* {{{ proto PixelWand MagickGetImageBackgroundColor( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagebackgroundcolor )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_DO_BOOL_FUNC_RET_PIXELWAND( MagickGetImageBackgroundColor );
}
/* }}} */

/* {{{ proto string MagickGetImageBlob( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageblob )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *orig_filename = (char *) NULL, *orig_img_format = (char *) NULL;
	long img_idx;
	int filename_valid;
	size_t blob_len = 0;
	MagickBooleanType img_had_format;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );
	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	/* Saves the current active image index, as well as performs a shortcut check for any exceptions,
	   as they will occur if magick_wand contains no images
	*/
	img_idx = (long) MagickGetIteratorIndex( magick_wand );
	if ( MagickGetExceptionType(magick_wand) != UndefinedException  ) {
		RETURN_FALSE;
	}
	MagickClearException( magick_wand );

	MW_ENSURE_IMAGE_HAS_FORMAT( magick_wand, img_idx, img_had_format, orig_img_format, "MagickGetImageBlob" );

	orig_filename = (char *) MagickGetImageFilename( magick_wand );
	filename_valid = !(orig_filename == (char *) NULL || *orig_filename == '\0');

	if ( filename_valid ) {
		MagickSetImageFilename( magick_wand, (char *) NULL );
	}

	MW_FUNC_RETVAL_STRING_L( magick_wand, MagickWand, MagickGetImageBlob( magick_wand, &blob_len ), blob_len );

	if ( filename_valid ) {
		MagickSetImageFilename( magick_wand, orig_filename );
	}

	/* Check if current image had a format before function started */
	if ( img_had_format == MagickFalse ) {

		MW_DEBUG_NOTICE_EX( "Attempting to reset image #%d's image format", img_idx );

		/* If not, it must have been changed to the MagickWand's format, so set it back (probably to nothing) */
		if ( MagickSetImageFormat( magick_wand, orig_img_format ) == MagickFalse ) {

			MW_DEBUG_NOTICE_EX(	"FAILURE: could not reset image #%d's image format", img_idx );

			/* C API could not reset set the current image's format back to its original state:
			   output error, with reason
			*/
			MW_API_FUNC_FAIL_CHECK_WAND_ERROR_EX_1(	magick_wand, MagickWand,
													"unable to set the image at MagickWand index %ld back to "  \
													"its original image format",
													img_idx
			);
		}
	}

	MW_FREE_MAGICK_MEM( char *, orig_img_format );
	MW_FREE_MAGICK_MEM( char *, orig_filename );
}
/* }}} */

/* {{{ proto MagickWand MagickGetImageClipMask( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageclipmask )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickGetImageClipMask( magick_wand ) );
}
/* }}} */

/* {{{ proto string MagickGetImagesBlob( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagesblob )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *orig_filename = (char *) NULL;
	long img_idx;
	int filename_valid;
	size_t blob_len = 0;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );
	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	/* Saves the current active image index, as well as performs a shortcut check for any exceptions,
	   as they will occur if magick_wand contains no images
	*/
	img_idx = (long) MagickGetIteratorIndex( magick_wand );
	if ( MagickGetExceptionType(magick_wand) != UndefinedException  ) {
		RETURN_FALSE;
	}
	MagickClearException( magick_wand );

	MW_CHECK_FOR_WAND_FORMAT( magick_wand, "MagickGetImagesBlob" );

	orig_filename = (char *) MagickGetFilename( magick_wand );
	filename_valid = !(orig_filename == (char *) NULL || *orig_filename == '\0');

	if ( filename_valid ) {
		MagickSetFilename( magick_wand, (char *) NULL );
	}

	MW_FUNC_RETVAL_STRING_L( magick_wand, MagickWand, MagickGetImagesBlob( magick_wand, &blob_len ), blob_len );

	if ( filename_valid ) {
		MagickSetFilename( magick_wand, orig_filename );
	}

	MW_FREE_MAGICK_MEM( char *, orig_filename );
}
/* }}} */

/* {{{ proto array MagickGetImageBluePrimary( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageblueprimary )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_CHK_BOOL_RET_2_IDX_DBL_ARR( MagickGetImageBluePrimary( magick_wand, &x, &y ), x, y );
}
/* }}} */

/* {{{ proto PixelWand MagickGetImageBorderColor( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagebordercolor )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_DO_BOOL_FUNC_RET_PIXELWAND( MagickGetImageBorderColor );
}
/* }}} */

/* {{{ proto array MagickGetImageChannelMean( MagickWand magick_wand, int channel_type )
*/
PHP_FUNCTION( magickgetimagechannelmean )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long channel;
	double mean, standard_deviation;

	MW_GET_2_ARG( "rl", &magick_wand_rsrc_zvl_p, &channel );

	MW_CHECK_CONSTANT( ChannelType, channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_CHK_BOOL_RET_2_IDX_DBL_ARR( MagickGetImageChannelMean( magick_wand, (ChannelType) channel, &mean, &standard_deviation ),
								   mean, standard_deviation );
}
/* }}} */

/* {{{ proto PixelWand MagickGetImageColormapColor( MagickWand magick_wand, float index )
*/
PHP_FUNCTION( magickgetimagecolormapcolor )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *colormap_wnd;
	zval *magick_wand_rsrc_zvl_p;
	double index;

	MW_GET_2_ARG( "rd", &magick_wand_rsrc_zvl_p, &index );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	colormap_wnd = (PixelWand *) NewPixelWand();

	if ( colormap_wnd == (PixelWand *) NULL ) {
		MW_SPIT_FATAL_ERR( "unable to create necessary PixelWand" );
		return;
	}
	else {
		if ( MagickGetImageColormapColor( magick_wand, (unsigned long) index, colormap_wnd ) == MagickTrue ) {
			MW_SET_RET_WAND_RSRC( PixelWand, colormap_wnd );
		}
		else {
			colormap_wnd = DestroyPixelWand( colormap_wnd );
			RETURN_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto float MagickGetImageColors( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagecolors )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetImageColors );
}
/* }}} */

/* {{{ proto int MagickGetImageColorspace( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagecolorspace )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetImageColorspace );
}
/* }}} */

/* {{{ proto int MagickGetImageCompose( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagecompose )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetImageCompose );
}
/* }}} */

/* {{{ proto int MagickGetImageCompression( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagecompression )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetImageCompression );
}
/* }}} */

/* {{{ proto float MagickGetImageCompressionQuality( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagecompressionquality )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetImageCompressionQuality );
}
/* }}} */

/* {{{ proto float MagickGetImageDelay( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagedelay )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetImageDelay );
}
/* }}} */

/* {{{ proto float MagickGetImageDepth( MagickWand magick_wand [, int channel_type] )
*/
PHP_FUNCTION( magickgetimagedepth )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long channel = -1;
	unsigned long channel_depth;

	MW_GET_2_ARG( "r|l", &magick_wand_rsrc_zvl_p, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		channel_depth = (unsigned long) MagickGetImageDepth( magick_wand );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		channel_depth = (unsigned long) MagickGetImageChannelDepth( magick_wand, (ChannelType) channel );
	}

	if ( MagickGetExceptionType(magick_wand) == UndefinedException ) {
		RETURN_LONG( (long) channel_depth );
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto float MagickGetImageDistortion( MagickWand magick_wand, MagickWand ref_wnd, MetricType metric [, ChannelType channel] )
*/
PHP_FUNCTION( magickgetimagedistortion )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand, *ref_wnd;
	zval *magick_wand_rsrc_zvl_p, *ref_wnd_rsrc_zvl_p;
	long metric, channel = -1;
	MagickBooleanType magick_return;
	double distortion;

	MW_GET_4_ARG( "rrl|l", &magick_wand_rsrc_zvl_p, &ref_wnd_rsrc_zvl_p, &metric, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );
	MW_GET_POINTER_FROM_RSRC( MagickWand, ref_wnd,  &ref_wnd_rsrc_zvl_p  );

	MW_CHECK_CONSTANT( MetricType, metric );

	if ( channel == -1 ) {
		magick_return = MagickGetImageDistortion( magick_wand, ref_wnd, (MetricType) metric, &distortion );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		magick_return = (MagickBooleanType) MagickGetImageChannelDistortion(	magick_wand, ref_wnd,
																				(ChannelType) channel, (MetricType) metric,
																				&distortion );
	}

	if ( magick_return == MagickTrue ) {
		RETURN_DOUBLE( distortion );
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto int MagickGetImageDispose( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagedispose )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetImageDispose );
}
/* }}} */

/* {{{ proto int MagickGetImageEndian( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageendian )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetImageEndian );
}
/* }}} */

/* {{{ proto string MagickGetImageProperty( MagickWand magick_wand, string key )
*/
PHP_FUNCTION( magickgetimageattribute )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *key, *value;
	int key_len;

	MW_GET_3_ARG( "rs", &magick_wand_rsrc_zvl_p, &key, &key_len );

	MW_CHECK_PARAM_STR_LEN( key_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	value = (char *) MagickGetImageProperty( magick_wand, key );

	if ( value == (char *) NULL || *value == '\0' ) {
		RETVAL_MW_EMPTY_STRING();
	}
	else {
		RETVAL_STRING( value, 1 );
	}

	MW_FREE_MAGICK_MEM( char *, value );
}
/* }}} */

/* {{{ proto array MagickGetImageExtrema( MagickWand magick_wand [, int channel_type] )
*/
PHP_FUNCTION( magickgetimageextrema )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long channel = -1;
	double minima, maxima;

	MW_GET_2_ARG( "r|l", &magick_wand_rsrc_zvl_p, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_CHK_BOOL_RET_2_IDX_DBL_ARR( MagickGetImageChannelRange( magick_wand, DefaultChannels, &minima, &maxima ),
									   minima, maxima );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_CHK_BOOL_RET_2_IDX_DBL_ARR( MagickGetImageChannelRange( magick_wand, (ChannelType) channel, &minima, &maxima ),
									   minima, maxima );
	}
}
/* }}} */

/* {{{ proto string MagickGetImageFilename( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagefilename )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_STRING( MagickWand,  MagickGetImageFilename );
}
/* }}} */

/* {{{ proto string MagickGetImageFormat( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageformat )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *img_format;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	img_format = (char *) MagickGetImageFormat( magick_wand );

	if ( img_format == (char *) NULL || *img_format == '\0' || *img_format == '*' ) {

		if ( MagickGetExceptionType(magick_wand) == UndefinedException ) {
			RETVAL_MW_EMPTY_STRING();
		}
		else {
			RETVAL_FALSE;
		}
	}
	else {
		RETVAL_STRING( img_format, 1 );
	}

	MW_FREE_MAGICK_MEM( char *, img_format );
}
/* }}} */

/* {{{ proto float MagickGetImageGamma( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagegamma )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetImageGamma );
}
/* }}} */

/* {{{ proto array MagickGetImageGreenPrimary( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagegreenprimary )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_CHK_BOOL_RET_2_IDX_DBL_ARR( MagickGetImageGreenPrimary( magick_wand, &x, &y ), x, y );
}
/* }}} */

/* {{{ proto float MagickGetImageHeight( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageheight )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetImageHeight );
}
/* }}} */

/* {{{ proto array MagickGetImageHistogram( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagehistogram )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	unsigned long num_colors;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_CHECK_ERR_RET_RESOURCE_ARR( PixelWand, MagickGetImageHistogram( magick_wand, &num_colors ), num_colors, magick_wand );
}
/* }}} */

/* {{{ proto int MagickGetIteratorIndex( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageindex )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetIteratorIndex );
}
/* }}} */

/* {{{ proto int MagickGetImageInterlaceScheme( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageinterlacescheme )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetImageInterlaceScheme );
}
/* }}} */

/* {{{ proto float MagickGetImageIterations( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageiterations )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetImageIterations );
}
/* }}} */

/* {{{ proto PixelWand MagickGetImageMatteColor( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagemattecolor )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_DO_BOOL_FUNC_RET_PIXELWAND( MagickGetImageMatteColor );
}
/* }}} */

/* {{{ proto string MagickGetImageMimeType( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagemimetype )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MIME_TYPE( MagickGetImageFormat );
}
/* }}} */

/* {{{ proto string MagickGetMimeType( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetmimetype )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MIME_TYPE( MagickGetFormat );
}
/* }}} */

/* {{{ proto array MagickGetImagePage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagepage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	unsigned long width, height;
	long x, y;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( MagickGetImagePage( magick_wand, &width, &height, &x, &y ) == MagickTrue ) {
		array_init( return_value );

		if (	add_index_double( return_value, 0, width  ) == FAILURE
			||	add_index_double( return_value, 1, height ) == FAILURE
			||	add_index_long(   return_value, 2, x      ) == FAILURE
			||	add_index_long(   return_value, 3, y      ) == FAILURE )
		{
			MW_SPIT_FATAL_ERR( "error adding a value to the array to be returned" );
			return;
		}
	}
	else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto PixelWand MagickGetImagePixelColor( MagickWand magick_wand, long x, long y )
*/
PHP_FUNCTION( magickgetimagepixelcolor )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *color_wnd;
	zval *magick_wand_rsrc_zvl_p;
	long x, y;

	MW_GET_3_ARG( "rll", &magick_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	color_wnd = (PixelWand *) NewPixelWand();

	if ( color_wnd == (PixelWand *) NULL ) {
		MW_SPIT_FATAL_ERR( "unable to create necessary PixelWand" );
		return;
	}
	else {
		if ( MagickGetImagePixelColor( magick_wand, x, y, color_wnd ) == MagickTrue ) {
			MW_SET_RET_WAND_RSRC( PixelWand, color_wnd );
		}
		else {
			color_wnd = DestroyPixelWand( color_wnd );
			RETURN_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto array MagickExportImagePixels( MagickWand magick_wand, int x_offset, int y_offset, float columns, float rows, string map, int storage_type )
*/
PHP_FUNCTION( magickgetimagepixels )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long x_offset, y_offset, php_storage;
	double columns, rows;
	char *map, *c;
	int map_len, pixel_array_length, i;
	StorageType storage;

	MW_GET_8_ARG( "rllddsl",
				  &magick_wand_rsrc_zvl_p,
				  &x_offset, &y_offset,
				  &columns, &rows,
				  &map, &map_len,
				  &php_storage );

	MW_CHECK_PARAM_STR_LEN( map_len );

	for ( i = 0; i < map_len; i++ ) {
		c = &(map[i]);

		if ( !( *c == 'A' || *c == 'a' ||
				*c == 'B' || *c == 'b' ||
				*c == 'C' || *c == 'c' ||
				*c == 'G' || *c == 'g' ||
				*c == 'I' || *c == 'i' ||
				*c == 'K' || *c == 'k' ||
				*c == 'M' || *c == 'm' ||
				*c == 'O' || *c == 'o' ||
				*c == 'P' || *c == 'p' ||
				*c == 'R' || *c == 'r' ||
				*c == 'Y' || *c == 'y' ) ) {
			MW_SPIT_FATAL_ERR( "map parameter string can only contain the letters A B C G I K M O P R Y; see docs for details" );
			return;
		}
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	pixel_array_length = (int) ( columns * rows * map_len );

	storage = (StorageType) php_storage;

#define PRV_RET_PIXEL_ARRAY( type, return_type )  \
{   type *pixel_array;  \
	MW_ARR_ECALLOC( type, pixel_array, pixel_array_length );  \
	if ( MagickExportImagePixels( magick_wand,  \
							   x_offset, y_offset,  \
							   (unsigned long) columns, (unsigned long) rows,  \
							   map,  \
							   storage,  \
							   (void *) pixel_array ) == MagickFalse )  \
	{  \
		RETVAL_FALSE;  \
	}  \
	else {  \
		array_init( return_value );  \
		for ( i = 0; i < pixel_array_length; i++ ) {  \
			MW_SET_1_RET_ARR_VAL( add_next_index_##return_type ( return_value, (return_type) (pixel_array[i]) ) );  \
		}  \
	}  \
	efree( pixel_array );  \
}

	switch ( storage ) {
		 case CharPixel		: PRV_RET_PIXEL_ARRAY( unsigned char,  long   );  break;
		 case ShortPixel	: PRV_RET_PIXEL_ARRAY( unsigned short, long   );  break;
		 case IntegerPixel	: PRV_RET_PIXEL_ARRAY( unsigned int,   double );  break;
		 case LongPixel		: PRV_RET_PIXEL_ARRAY( unsigned long,  double );  break;
		 case FloatPixel	: PRV_RET_PIXEL_ARRAY( float,          double );  break;
		 case DoublePixel	: PRV_RET_PIXEL_ARRAY( double,         double );  break;

		 default : MW_SPIT_FATAL_ERR( "Invalid StorageType argument supplied to function" );
				   return;
	}
}
/* }}} */

/* {{{ proto string MagickGetImageProfile( MagickWand magick_wand, string name )
*/
PHP_FUNCTION( magickgetimageprofile )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *name;
	int name_len;
	size_t profile_len = 0;

	MW_GET_3_ARG( "rs", &magick_wand_rsrc_zvl_p, &name, &name_len );

	MW_CHECK_PARAM_STR_LEN( name_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_FUNC_RETVAL_STRING_L( magick_wand, MagickWand, MagickGetImageProfile( magick_wand, name, &profile_len ), profile_len );
}
/* }}} */

/* {{{ proto string MagickGetImageProperty( MagickWand magick_wand, string key )
*/
PHP_FUNCTION( magickgetimageproperty )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *key, *value;
	int key_len;

	MW_GET_3_ARG( "rs", &magick_wand_rsrc_zvl_p, &key, &key_len );

	MW_CHECK_PARAM_STR_LEN( key_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	value = (char *) MagickGetImageProperty( magick_wand, key );

	if ( value == (char *) NULL || *value == '\0' ) {
		RETVAL_MW_EMPTY_STRING();
	}
	else {
		RETVAL_STRING( value, 1 );
	}

	MW_FREE_MAGICK_MEM( char *, value );
}
/* }}} */

/* {{{ proto array MagickGetImageRedPrimary( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageredprimary )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_CHK_BOOL_RET_2_IDX_DBL_ARR( MagickGetImageRedPrimary( magick_wand, &x, &y ), x, y );
}
/* }}} */

/* {{{ proto MagickWand MagickGetImageRegion( MagickWand magick_wand, int width, int height, int x, int y )
*/
PHP_FUNCTION( magickgetimageregion )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double width, height;
	long x, y;

	MW_GET_5_ARG( "rddll", &magick_wand_rsrc_zvl_p, &width, &height, &x, &y );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC(	MagickWand,
									MagickGetImageRegion(
											magick_wand, (unsigned long) width, (unsigned long) height, x, y ) );
}
/* }}} */

/* {{{ proto int MagickGetImageRenderingIntent( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagerenderingintent )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetImageRenderingIntent );
}
/* }}} */

/* {{{ proto array MagickGetImageResolution( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageresolution )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_CHK_BOOL_RET_2_IDX_DBL_ARR( MagickGetImageResolution( magick_wand, &x, &y ), x, y );
}
/* }}} */

/* {{{ proto float MagickGetImageScene( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagescene )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetImageScene );
}
/* }}} */

/* {{{ proto string MagickGetImageSignature( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagesignature )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_STRING( MagickWand, MagickGetImageSignature );
}
/* }}} */

/* {{{ proto int MagickGetImageLength( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagesize )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetImageSize );
}
/* }}} */

/* {{{ proto float MagickGetImageTicksPerSecond( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagetickspersecond )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetImageTicksPerSecond );
}
/* }}} */

/* {{{ proto float MagickGetImageTotalInkDensity( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagetotalinkdensity )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetImageTotalInkDensity );
}
/* }}} */

/* {{{ proto int MagickGetImageType( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagetype )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetImageType );
}
/* }}} */

/* {{{ proto int MagickGetImageUnits( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimageunits )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetImageUnits );
}
/* }}} */

/* {{{ proto int MagickGetImageVirtualPixelMethod( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagevirtualpixelmethod )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetImageVirtualPixelMethod );
}
/* }}} */

/* {{{ proto array MagickGetImageWhitePoint( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagewhitepoint )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_CHK_BOOL_RET_2_IDX_DBL_ARR( MagickGetImageWhitePoint( magick_wand, &x, &y ), x, y );
}
/* }}} */

/* {{{ proto float MagickGetImageWidth( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetimagewidth )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetImageWidth );
}
/* }}} */

/* {{{ proto int MagickGetInterlaceScheme( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetinterlacescheme )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_LONG( MagickWand, MagickGetInterlaceScheme );
}
/* }}} */

/* {{{ proto float MagickGetNumberImages( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetnumberimages )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( MagickWand, MagickGetNumberImages );
}
/* }}} */

/* {{{ proto string MagickGetOption( MagickWand magick_wand, string key )
*/
PHP_FUNCTION( magickgetoption )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *key, *value;
	int key_len;

	MW_GET_3_ARG( "rs", &magick_wand_rsrc_zvl_p, &key, &key_len );

	MW_CHECK_PARAM_STR_LEN( key_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	value = (char *) MagickGetOption( magick_wand, key );

	if ( value == (char *) NULL || *value == '\0' ) {
		RETVAL_MW_EMPTY_STRING();
	}
	else {
		RETVAL_STRING( value, 1 );
	}

	MW_FREE_MAGICK_MEM( char *, value );
}
/* }}} */

/* {{{ proto string MagickGetPackageName( void )
*/
PHP_FUNCTION( magickgetpackagename )
{
	MW_PRINT_DEBUG_INFO

	char *pckg_name;

/* No freeing of pkg_name after this call, because string returned is a constant string, ie "..."
*/
	pckg_name = (char *) MagickGetPackageName();

	RETURN_STRING( pckg_name, 1 );
}
/* }}} */

/* {{{ proto array MagickGetPage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetpage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	unsigned long width, height;
	long x, y;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( MagickGetPage( magick_wand, &width, &height, &x, &y ) == MagickTrue ) {
		array_init( return_value );

		if (	add_index_double( return_value, 0, width  ) == FAILURE
			||	add_index_double( return_value, 1, height ) == FAILURE
			||	add_index_long(   return_value, 2, x      ) == FAILURE
			||	add_index_long(   return_value, 3, y      ) == FAILURE )
		{
			MW_SPIT_FATAL_ERR( "error adding a value to the array to be returned" );
			return;
		}
	}
	else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto int MagickGetQuantumDepth( void )
*/
PHP_FUNCTION( magickgetquantumdepth )
{
	MW_PRINT_DEBUG_INFO

	unsigned long depth_num;

	/* can cast the return string as void, because it is a string constant,
		so I do not have to worry about deallocating the memory associated
		with it -- it is done automatically
	*/
	(void *) MagickGetQuantumDepth( &depth_num );

	RETURN_LONG( (long) depth_num );
}
/* }}} */

/* {{{ proto int MagickGetQuantumRange( void )
*/
PHP_FUNCTION( magickgetquantumrange )
{
	MW_PRINT_DEBUG_INFO

	unsigned long range_num;

	/* can cast the return string as void, because it is a string constant,
		so I do not have to worry about deallocating the memory associated
		with it -- it is done automatically
	*/
	(void *) MagickGetQuantumRange( &range_num );

	RETURN_LONG( (long) range_num );
}
/* }}} */

/* {{{ proto string MagickGetReleaseDate( void )
*/
PHP_FUNCTION( magickgetreleasedate )
{
	MW_PRINT_DEBUG_INFO

	char *rel_date;

/* No freeing of rel_date after this call, because string returned is a constant string, ie "..."
*/
	rel_date = (char *) MagickGetReleaseDate();
	RETURN_STRING( rel_date, 1 );
}
/* }}} */

/* {{{ proto float MagickGetResource( int resource_type )
*/
PHP_FUNCTION( magickgetresource )
{
	MW_PRINT_DEBUG_INFO

	long rsrc_type;

	MW_GET_1_ARG( "l", &rsrc_type );

	MW_CHECK_CONSTANT( ResourceType, rsrc_type );

	RETURN_DOUBLE( (double) MagickGetResource( (ResourceType) rsrc_type ) );
}
/* }}} */

/* {{{ proto float MagickGetResourceLimit( int resource_type )
*/
PHP_FUNCTION( magickgetresourcelimit )
{
	MW_PRINT_DEBUG_INFO

	long rsrc_type;

	MW_GET_1_ARG( "l", &rsrc_type );

	MW_CHECK_CONSTANT( ResourceType, rsrc_type );

	RETURN_DOUBLE( (double) MagickGetResourceLimit( (ResourceType) rsrc_type ) );
}
/* }}} */

/* {{{ proto array MagickGetSamplingFactors( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetsamplingfactors )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE_ARR( MagickWand, MagickGetSamplingFactors );
}
/* }}} */

/* {{{ proto bool MagickGetWandSize( MagickWand magick_wand )
*/
PHP_FUNCTION( magickgetwandsize )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	unsigned long columns, rows;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_CHK_BOOL_RET_2_IDX_DBL_ARR( MagickGetSize( magick_wand, &columns, &rows ), (double) columns, (double) rows );
}
/* }}} */

/* {{{ proto array MagickGetVersion( void )
*/
PHP_FUNCTION( magickgetversion )
{
	MW_PRINT_DEBUG_INFO

	char *version_str;
	unsigned long version_num;

/*  CANNOT MagickRelinquishMemory() the string returned from MagickGetVersion(), since the
	char * returned is actually a string constant, which will be automatically dealt with
	-- actually causes MEGA error if you try to clean it up manually
*/
	version_str = (char *) MagickGetVersion( &version_num );

	array_init( return_value );

	MW_SET_2_RET_ARR_VALS( add_next_index_string( return_value, version_str, 1 ),
						   add_next_index_long( return_value, (long) version_num ) );
}
/* }}} */

/* {{{ proto bool MagickGetVersionNumber( void )
*/
PHP_FUNCTION( magickgetversionnumber )
{
	MW_PRINT_DEBUG_INFO

	unsigned long version_num;

	/*  see comment in magickgetversion() above */

	/* can cast the return string as void, because it is a string constant,
		so I do not have to worry about deallocating the memory associated
		with it -- it is done automatically
	*/
	(void *) MagickGetVersion( &version_num );

	RETURN_LONG( (long) version_num );
}
/* }}} */

/* {{{ proto bool MagickGetVersionString( void )
*/
PHP_FUNCTION( magickgetversionstring )
{
	MW_PRINT_DEBUG_INFO

	char *version_str;

	/*  see comment in magickgetversion() above */

	version_str = (char *) MagickGetVersion( (unsigned long *) NULL );

	RETURN_STRING( version_str, 1 );
}
/* }}} */

/* {{{ proto bool MagickHasNextImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickhasnextimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickHasNextImage );
}
/* }}} */

/* {{{ proto bool MagickHasPreviousImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickhaspreviousimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickHasPreviousImage );
}
/* }}} */

/* {{{ proto bool MagickImplodeImage( MagickWand magick_wand, float amount )
*/
PHP_FUNCTION( magickimplodeimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_DOUBLE_RET_BOOL( MagickImplodeImage );
}
/* }}} */

/* {{{ proto bool MagickLabelImage( MagickWand magick_wand, string label )
*/
PHP_FUNCTION( magicklabelimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_AND_STRING_RETVAL_FUNC_BOOL( MagickWand, MagickLabelImage );
}
/* }}} */

/* {{{ proto bool MagickLevelImage( MagickWand magick_wand, double black_point, float gamma, float white_point [, int channel_type] )
*/
PHP_FUNCTION( magicklevelimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double black_point, gamma, white_point;
	long channel = -1;

	MW_GET_5_ARG( "rddd|l", &magick_wand_rsrc_zvl_p, &black_point, &gamma, &white_point, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickLevelImage( magick_wand, black_point, gamma, white_point ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickLevelImageChannel( magick_wand, (ChannelType) channel, black_point, gamma, white_point ) );
	}
}
/* }}} */

/* {{{ proto bool MagickMagnifyImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickmagnifyimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickMagnifyImage );
}
/* }}} */

/* {{{ proto bool MagickMapImage( MagickWand magick_wand, MagickWand map_wand, bool dither )
*/
PHP_FUNCTION( magickmapimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand, *map_wnd;
	zval *magick_wand_rsrc_zvl_p, *map_wnd_rsrc_zvl_p;
	zend_bool dither = FALSE;

	MW_GET_3_ARG( "rr|b", &magick_wand_rsrc_zvl_p, &map_wnd_rsrc_zvl_p, &dither );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, map_wnd, &map_wnd_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickRemapImage( magick_wand, map_wnd, MW_MK_MGCK_BOOL(dither) ) );
}
/* }}} */

/* {{{ proto bool MagickMatteFloodfillImage( MagickWand magick_wand, float alpha, float fuzz, mixed bordercolor_pixel_wand, int x, int y )
*/
PHP_FUNCTION( magickmattefloodfillimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *fillcolor_pixel_wand, *bordercolor_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;
	double alpha, fuzz;
	long x, y;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 6),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, an alpha value, a fuzz value, "  \
							"a bordercolor PixelWand resource (or ImageMagick color string), "  \
							"and the x and y ordinates of the starting pixel" );

	convert_to_double_ex( zvl_pp_args_arr[1] );
	alpha =  Z_DVAL_PP( zvl_pp_args_arr[1] );

	convert_to_double_ex( zvl_pp_args_arr[2] );
	fuzz =     Z_DVAL_PP( zvl_pp_args_arr[2] );

	convert_to_long_ex( zvl_pp_args_arr[4] );
	x =      Z_LVAL_PP( zvl_pp_args_arr[4] );

	convert_to_long_ex( zvl_pp_args_arr[5] );
	y =      Z_LVAL_PP( zvl_pp_args_arr[5] );

	if ( Z_TYPE_PP( zvl_pp_args_arr[3] ) == IS_RESOURCE ) {

		if (  (   MW_FETCH_RSRC( PixelWand, bordercolor_pixel_wand, zvl_pp_args_arr[3] ) == MagickFalse
			   && MW_FETCH_RSRC( PixelIteratorPixelWand, bordercolor_pixel_wand, zvl_pp_args_arr[3] ) == MagickFalse )
			|| IsPixelWand( bordercolor_pixel_wand ) == MagickFalse )
		{
			MW_SPIT_FATAL_ERR( "invalid resource type as argument #4; a PixelWand resource is required" );
			efree( zvl_pp_args_arr );
			return;
		}
		is_script_pixel_wand = 1;
	}
	else {
		if ( Z_TYPE_PP( zvl_pp_args_arr[3] ) == IS_NULL ) {
			is_script_pixel_wand = 1;
			bordercolor_pixel_wand = (PixelWand *) NULL;
		}
		else {
			is_script_pixel_wand = 0;

			bordercolor_pixel_wand = (PixelWand *) NewPixelWand();

			if ( bordercolor_pixel_wand == (PixelWand *) NULL ) {
				MW_SPIT_FATAL_ERR( "unable to create necessary PixelWand" );
				efree( zvl_pp_args_arr );
				return;
			}
			convert_to_string_ex( zvl_pp_args_arr[3] );

			if ( Z_STRLEN_PP( zvl_pp_args_arr[3] ) > 0 ) {

				if ( PixelSetColor( bordercolor_pixel_wand, Z_STRVAL_PP( zvl_pp_args_arr[3] ) ) == MagickFalse ) {

					MW_API_FUNC_FAIL_CHECK_WAND_ERROR(	bordercolor_pixel_wand, PixelWand,
														"could not set PixelWand to desired fill color"
					);

					bordercolor_pixel_wand = DestroyPixelWand( bordercolor_pixel_wand );
					efree( zvl_pp_args_arr );

					return;
				}
			}
		}
	}

  fillcolor_pixel_wand = (PixelWand *) NewPixelWand();

  PixelSetAlpha( fillcolor_pixel_wand, alpha );
	MW_BOOL_FUNC_RETVAL_BOOL( MagickFloodfillPaintImage( magick_wand, OpacityChannel, fillcolor_pixel_wand, fuzz, bordercolor_pixel_wand, x, y, MagickFalse ) );

  fillcolor_pixel_wand = DestroyPixelWand( fillcolor_pixel_wand );
	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		bordercolor_pixel_wand = DestroyPixelWand( bordercolor_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickMedianFilterImage( MagickWand magick_wand, float radius )
*/
PHP_FUNCTION( magickmedianfilterimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_DOUBLE_RET_BOOL( MagickMedianFilterImage );
}
/* }}} */

/* {{{ proto bool MagickMinifyImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickminifyimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickMinifyImage );
}
/* }}} */

/* {{{ proto bool MagickModulateImage( MagickWand magick_wand, float brightness, float saturation, float hue )
*/
PHP_FUNCTION( magickmodulateimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double brightness, saturation, hue;

	MW_GET_4_ARG( "rddd", &magick_wand_rsrc_zvl_p, &brightness, &saturation, &hue );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickModulateImage( magick_wand, brightness, saturation, hue ) );
}
/* }}} */

/* {{{ proto MagickWand MagickMontageImage( MagickWand magick_wand, DrawingWand draw_wand, string tile_geometry, string thumbnail_geometry, int montage_mode, string frame )
*/
PHP_FUNCTION( magickmontageimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	DrawingWand *draw_wand;
	zval *magick_wand_rsrc_zvl_p, *draw_wand_rsrc_zvl_p;
	char	*script_tile_geom,  *tile_geom,
			*script_thumb_geom, *thumb_geom,
			*script_frame_geom, *frame_geom;
	int tile_geom_len, thumb_geom_len, frame_geom_len;
	long mode;

	MW_GET_9_ARG(	"rrssls",
					&magick_wand_rsrc_zvl_p, &draw_wand_rsrc_zvl_p,
					&script_tile_geom, &tile_geom_len,
					&script_thumb_geom, &thumb_geom_len,
					&mode,
					&script_frame_geom, &frame_geom_len );

	MW_CHECK_GEOMETRY_STR_LENGTHS( (tile_geom_len == 0 && thumb_geom_len == 0 && frame_geom_len == 0) );

	if ( tile_geom_len < 1 ) {
		tile_geom = (char *) NULL;
	}
	else {
		tile_geom = script_tile_geom;
	}

	if ( thumb_geom_len < 1 ) {
		thumb_geom = (char *) NULL;
	}
	else {
		thumb_geom = script_thumb_geom;
	}

	if ( frame_geom_len < 1 ) {
		frame_geom = (char *) NULL;
	}
	else {
		frame_geom = script_frame_geom;
	}

	MW_CHECK_CONSTANT( MontageMode, mode );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( DrawingWand, draw_wand, &draw_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC(
		MagickWand,
		MagickMontageImage( magick_wand, draw_wand, tile_geom, thumb_geom, (MontageMode) mode, frame_geom ) );
}
/* }}} */

/* {{{ proto MagickWand MagickMorphImages( MagickWand magick_wand, float number_frames )
*/
PHP_FUNCTION( magickmorphimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double num_frames;

	MW_GET_2_ARG( "rd", &magick_wand_rsrc_zvl_p, &num_frames );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickMorphImages( magick_wand, (unsigned long) num_frames ) );
}
/* }}} */

/* {{{ proto MagickWand MagickMosaicImages( MagickWand magick_wand )
*/
PHP_FUNCTION( magickmosaicimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickMosaicImages( magick_wand ) );
}
/* }}} */

/* {{{ proto bool MagickMotionBlurImage( MagickWand magick_wand, float radius, float sigma, float angle )
*/
PHP_FUNCTION( magickmotionblurimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double radius, sigma, angle;

	MW_GET_4_ARG( "rddd", &magick_wand_rsrc_zvl_p, &radius, &sigma, &angle );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickMotionBlurImage( magick_wand, radius, sigma, angle ) );
}
/* }}} */

/* {{{ proto bool MagickNegateImage( MagickWand magick_wand [, bool only_the_gray [, int channel_type]] )
*/
PHP_FUNCTION( magicknegateimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	zend_bool only_the_gray = FALSE;
	long channel = -1;

	MW_GET_3_ARG( "r|bl", &magick_wand_rsrc_zvl_p, &only_the_gray, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickNegateImage( magick_wand, MW_MK_MGCK_BOOL(only_the_gray) ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickNegateImageChannel( magick_wand, (ChannelType) channel, MW_MK_MGCK_BOOL(only_the_gray) ) );
	}
}
/* }}} */

/* {{{ proto bool MagickNewImage( MagickWand magick_wand, float width, float height [, string imagemagick_col_str] )
*/
PHP_FUNCTION( magicknewimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *bg_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;
	double width, height;
	long cur_idx;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count < 3 || arg_count > 4),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, a desired image width and height, and an optional " \
							"background color PixelWand resource or ImageMagick color string" );

	convert_to_double_ex( zvl_pp_args_arr[1] );
	convert_to_double_ex( zvl_pp_args_arr[2] );

	width  = Z_DVAL_PP( zvl_pp_args_arr[1] );
	height = Z_DVAL_PP( zvl_pp_args_arr[2] );

	if ( width < 1 || height < 1 ) {
		MW_SPIT_FATAL_ERR( "cannot create an image smaller than 1 x 1 pixels in area" );
		efree( zvl_pp_args_arr );
		return;
	}

	if ( arg_count == 4 && Z_TYPE_PP( zvl_pp_args_arr[3] ) == IS_RESOURCE ) {

		if (  (   MW_FETCH_RSRC( PixelWand, bg_pixel_wand, zvl_pp_args_arr[3] ) == MagickFalse
			   && MW_FETCH_RSRC( PixelIteratorPixelWand, bg_pixel_wand, zvl_pp_args_arr[3] ) == MagickFalse )
			|| IsPixelWand( bg_pixel_wand ) == MagickFalse )
		{
			MW_SPIT_FATAL_ERR( "invalid resource type as fourth argument; a PixelWand resource is required" );
			efree( zvl_pp_args_arr );
			return;
		}
		is_script_pixel_wand = 1;
	}
	else {
		is_script_pixel_wand = 0;

		bg_pixel_wand = (PixelWand *) NewPixelWand();

		if ( bg_pixel_wand == (PixelWand *) NULL ) {
			MW_SPIT_FATAL_ERR( "unable to create necessary background color PixelWand" );
			efree( zvl_pp_args_arr );
			return;
		}
		if ( arg_count == 4 ) {
			convert_to_string_ex( zvl_pp_args_arr[3] );

			if ( Z_STRLEN_PP( zvl_pp_args_arr[3] ) > 0 ) {

				if ( PixelSetColor( bg_pixel_wand, Z_STRVAL_PP( zvl_pp_args_arr[3] ) ) == MagickFalse ) {

					MW_API_FUNC_FAIL_CHECK_WAND_ERROR(	bg_pixel_wand, PixelWand,
														"could not set PixelWand to desired fill color"
					);

					bg_pixel_wand = DestroyPixelWand( bg_pixel_wand );
					efree( zvl_pp_args_arr );

					return;
				}
			}
		}
	}

	cur_idx = (long) MagickGetIteratorIndex( magick_wand );
	if ( MagickGetExceptionType(magick_wand) != UndefinedException ) {
		cur_idx = -1;
	}
	MagickClearException( magick_wand );

	if (   MagickNewImage( magick_wand, (unsigned long) width, (unsigned long) height, bg_pixel_wand ) == MagickTrue
		&& MagickSetIteratorIndex( magick_wand, (cur_idx + 1) ) == MagickTrue )
	{
		RETVAL_TRUE;
	}
	else {
		MW_API_FUNC_FAIL_CHECK_WAND_ERROR( magick_wand, MagickWand, "unable to create new image of desired color" );
	}

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		bg_pixel_wand = DestroyPixelWand( bg_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickNextImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magicknextimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickNextImage );
}
/* }}} */

/* {{{ proto bool MagickNormalizeImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magicknormalizeimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickNormalizeImage );
}
/* }}} */

/* {{{ proto bool MagickOilPaintImage( MagickWand magick_wand, float radius )
*/
PHP_FUNCTION( magickoilpaintimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_DOUBLE_RET_BOOL( MagickOilPaintImage );
}
/* }}} */

/* {{{ proto MagickWand MagickOrderedPosterizeImage( MagickWand magick_wand, string threshold-map [, int channel_type] )
*/
PHP_FUNCTION( magickorderedposterizeimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *threshold_map;
	int map_len;
	long channel = -1;

	MW_GET_4_ARG( "rs|l", &magick_wand_rsrc_zvl_p, &threshold_map, &map_len, &channel );

	MW_CHECK_PARAM_STR_LEN( map_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickOrderedPosterizeImage( magick_wand, threshold_map ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickOrderedPosterizeImageChannel( magick_wand, (ChannelType) channel, threshold_map ) );
	}
}
/* }}} */

/* {{{ proto bool MagickFloodfillPaintImage( MagickWand magick_wand, mixed fillcolor_pixel_wand, float fuzz, mixed bordercolor_pixel_wand, int x, int y [, int channel_type] )
*/
PHP_FUNCTION( magickfloodfillpaintimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *fillcolor_pixel_wand, *bordercolor_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand, is_script_pixel_wand_2;
	double fuzz;
	long x, y;
	long channel = -1;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 6) && (arg_count != 7),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, a fill color PixelWand resource (or ImageMagick "  \
							"color string), a fuzz value, NULL or a bordercolor PixelWand resource (or "  \
							"ImageMagick color string), and the x and y ordinates of the starting pixel" );

	convert_to_double_ex( zvl_pp_args_arr[2] );
	fuzz =     Z_DVAL_PP( zvl_pp_args_arr[2] );

	convert_to_long_ex( zvl_pp_args_arr[4] );
	x =      Z_LVAL_PP( zvl_pp_args_arr[4] );

	convert_to_long_ex( zvl_pp_args_arr[5] );
	y =      Z_LVAL_PP( zvl_pp_args_arr[5] );

  if (arg_count > 6)
   	channel =      Z_LVAL_PP( zvl_pp_args_arr[6] );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, fillcolor_pixel_wand, is_script_pixel_wand );

	if ( Z_TYPE_PP( zvl_pp_args_arr[3] ) == IS_RESOURCE ) {

		if (  (   MW_FETCH_RSRC( PixelWand, bordercolor_pixel_wand, zvl_pp_args_arr[3] ) == MagickFalse
			   && MW_FETCH_RSRC( PixelIteratorPixelWand, bordercolor_pixel_wand, zvl_pp_args_arr[3] ) == MagickFalse )
			|| IsPixelWand( bordercolor_pixel_wand ) == MagickFalse )
		{
			MW_SPIT_FATAL_ERR( "invalid resource type as argument #4; a PixelWand resource is required" );
			efree( zvl_pp_args_arr );
			if ( !is_script_pixel_wand ) {
				fillcolor_pixel_wand = DestroyPixelWand( fillcolor_pixel_wand );
			}
			return;
		}
		is_script_pixel_wand_2 = 1;
	}
	else {
		if ( Z_TYPE_PP( zvl_pp_args_arr[3] ) == IS_NULL ) {
			is_script_pixel_wand_2 = 1;
			bordercolor_pixel_wand = (PixelWand *) NULL;
		}
		else {
			is_script_pixel_wand_2 = 0;

			bordercolor_pixel_wand = (PixelWand *) NewPixelWand();

			if ( bordercolor_pixel_wand == (PixelWand *) NULL ) {
				MW_SPIT_FATAL_ERR( "unable to create necessary PixelWand" );
				efree( zvl_pp_args_arr );
				if ( !is_script_pixel_wand ) {
					fillcolor_pixel_wand = DestroyPixelWand( fillcolor_pixel_wand );
				}
				return;
			}
			convert_to_string_ex( zvl_pp_args_arr[3] );

			if ( Z_STRLEN_PP( zvl_pp_args_arr[3] ) > 0 ) {

				if ( PixelSetColor( bordercolor_pixel_wand, Z_STRVAL_PP( zvl_pp_args_arr[3] ) ) == MagickFalse ) {

					MW_API_FUNC_FAIL_CHECK_WAND_ERROR(	bordercolor_pixel_wand, PixelWand,
														"could not set PixelWand to desired fill color"
					);

					bordercolor_pixel_wand = DestroyPixelWand( bordercolor_pixel_wand );
					efree( zvl_pp_args_arr );

					if ( !is_script_pixel_wand ) {
						fillcolor_pixel_wand = DestroyPixelWand( fillcolor_pixel_wand );
					}

					return;
				}
			}
		}
	}

  if (channel == -1)
    {
    	MW_BOOL_FUNC_RETVAL_BOOL( MagickFloodfillPaintImage( magick_wand, AllChannels, fillcolor_pixel_wand, fuzz, bordercolor_pixel_wand, x, y, MagickFalse ) );
    }
  else
    {
      MW_CHECK_CONSTANT( ChannelType, channel );
  	  MW_BOOL_FUNC_RETVAL_BOOL( MagickFloodfillPaintImage( magick_wand, channel, fillcolor_pixel_wand, fuzz, bordercolor_pixel_wand, x, y, MagickFalse ) );
    }
	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		fillcolor_pixel_wand = DestroyPixelWand( fillcolor_pixel_wand );
	}
	if ( !is_script_pixel_wand_2 ) {
		bordercolor_pixel_wand = DestroyPixelWand( bordercolor_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickOpaquePaintImage( MagickWand magick_wand, mixed target_pixel_wand, mixed fill_pixel_wand [, float fuzz] )
*/
PHP_FUNCTION( magickopaquepaintimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *target_pixel_wand, *fill_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand, is_script_pixel_wand_2;
	double fuzz = 0.0;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count < 3 || arg_count > 4),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, a target color PixelWand resource (or ImageMagick color "  \
							"string), a fill color PixelWand resource (or ImageMagick color "  \
							"string), and an optional fuzz value" );

	if ( arg_count == 4 ) {
		convert_to_double_ex( zvl_pp_args_arr[3] );
		fuzz =     Z_DVAL_PP( zvl_pp_args_arr[3] );
	}

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, target_pixel_wand, is_script_pixel_wand );

	if ( Z_TYPE_PP( zvl_pp_args_arr[2] ) == IS_RESOURCE ) {

		if (  (   MW_FETCH_RSRC( PixelWand, fill_pixel_wand, zvl_pp_args_arr[2] ) == MagickFalse
			   && MW_FETCH_RSRC( PixelIteratorPixelWand, fill_pixel_wand, zvl_pp_args_arr[2] ) == MagickFalse )
			|| IsPixelWand( fill_pixel_wand ) == MagickFalse )
		{
			MW_SPIT_FATAL_ERR( "invalid resource type as argument #3; a PixelWand resource is required" );
			efree( zvl_pp_args_arr );
			if ( !is_script_pixel_wand ) {
				target_pixel_wand = DestroyPixelWand( target_pixel_wand );
			}
			return;
		}
		is_script_pixel_wand_2 = 1;
	}
	else {
		is_script_pixel_wand_2 = 0;

		fill_pixel_wand = (PixelWand *) NewPixelWand();

		if ( fill_pixel_wand == (PixelWand *) NULL ) {
			MW_SPIT_FATAL_ERR( "unable to create necessary PixelWand" );
			efree( zvl_pp_args_arr );
			if ( !is_script_pixel_wand ) {
				target_pixel_wand = DestroyPixelWand( target_pixel_wand );
			}
			return;
		}
		convert_to_string_ex( zvl_pp_args_arr[2] );

		if ( Z_STRLEN_PP( zvl_pp_args_arr[2] ) > 0 ) {

			if ( PixelSetColor( fill_pixel_wand, Z_STRVAL_PP( zvl_pp_args_arr[2] ) ) == MagickFalse ) {

				MW_API_FUNC_FAIL_CHECK_WAND_ERROR(	fill_pixel_wand, PixelWand,
													"could not set PixelWand to desired fill color"
				);

				fill_pixel_wand = DestroyPixelWand( fill_pixel_wand );
				efree( zvl_pp_args_arr );
				if ( !is_script_pixel_wand ) {
					target_pixel_wand = DestroyPixelWand( target_pixel_wand );
				}

				return;
			}
		}
	}

	MW_BOOL_FUNC_RETVAL_BOOL( MagickOpaquePaintImage( magick_wand, target_pixel_wand, fill_pixel_wand, fuzz, MagickFalse ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		target_pixel_wand = DestroyPixelWand( target_pixel_wand );
	}
	if ( !is_script_pixel_wand_2 ) {
		fill_pixel_wand = DestroyPixelWand( fill_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickTransparentPaintImage( MagickWand magick_wand, mixed target_pixel_wand [, float alpha [, float fuzz]] )
*/
PHP_FUNCTION( magicktransparentpaintimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *target_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;
	double alpha = (double) 0.0, fuzz = 0.0;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count < 2 || arg_count > 4),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, a target color PixelWand resource (or ImageMagick color "  \
							"string), a fill color PixelWand resource (or ImageMagick color "  \
							"string), and an optional fuzz value" );

	if ( arg_count > 2 ) {
		convert_to_double_ex( zvl_pp_args_arr[2] );
		alpha =  Z_DVAL_PP( zvl_pp_args_arr[2] );

		if ( arg_count == 4 ) {
			convert_to_double_ex( zvl_pp_args_arr[3] );
			fuzz =     Z_DVAL_PP( zvl_pp_args_arr[3] );
		}
	}

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, target_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickTransparentPaintImage( magick_wand, target_pixel_wand, alpha, fuzz, MagickFalse ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		target_pixel_wand = DestroyPixelWand( target_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickPingImage( MagickWand magick_wand, string filename )
*/
PHP_FUNCTION( magickpingimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *filename, real_filename[MAXPATHLEN];
	int filename_len;

	MW_GET_3_ARG( "rs", &magick_wand_rsrc_zvl_p, &filename, &filename_len );

	MW_CHECK_PARAM_STR_LEN( filename_len );

	real_filename[0] = '\0';
	expand_filepath(filename, real_filename TSRMLS_CC);

	if ( real_filename[0] == '\0' || MW_FILE_FAILS_INI_TESTS( real_filename ) ) {
		zend_error( MW_E_ERROR, "%s(): PHP cannot read %s; possible php.ini restrictions",
								get_active_function_name(TSRMLS_C), real_filename );
		RETVAL_FALSE;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickPingImage( magick_wand, real_filename ) );
}
/* }}} */

/* {{{ proto bool MagickPosterizeImage( MagickWand magick_wand, float levels, bool dither )
*/
PHP_FUNCTION( magickposterizeimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double levels;
	zend_bool dither = FALSE;

	MW_GET_3_ARG( "rd|b", &magick_wand_rsrc_zvl_p, &levels, &dither );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickPosterizeImage( magick_wand, (unsigned long) levels, MW_MK_MGCK_BOOL(dither) ) );
}
/* }}} */

/* {{{ proto MagickWand MagickPreviewImages( MagickWand magick_wand, PreviewType preview )
*/
PHP_FUNCTION( magickpreviewimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long preview;

	MW_GET_2_ARG( "rl", &magick_wand_rsrc_zvl_p, &preview );

	MW_CHECK_CONSTANT( PreviewType, preview );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickPreviewImages( magick_wand, (PreviewType) preview ) );
}
/* }}} */

/* {{{ proto bool MagickPreviousImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickpreviousimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickPreviousImage );
}
/* }}} */

/* {{{ proto bool MagickProfileImage( MagickWand magick_wand, string name [, string profile] )
*/
PHP_FUNCTION( magickprofileimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *name, *profile = (char *) NULL;
	int name_len, profile_len = 0;

	MW_GET_5_ARG( "rs|s", &magick_wand_rsrc_zvl_p, &name, &name_len, &profile, &profile_len );

	MW_CHECK_PARAM_STR_LEN( name_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickProfileImage( magick_wand,
												  name,
												  (void *) (profile_len == 0 ? NULL : profile),
												  (size_t) profile_len
							  )
	);
}
/* }}} */

/* {{{ proto bool MagickQuantizeImage( MagickWand magick_wand, float number_colors, int colorspace_type, float treedepth, bool dither, bool measure_error )
*/
PHP_FUNCTION( magickquantizeimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double num_cols, treedepth;
	long col_space;
	zend_bool dither, measure_err;

	MW_GET_6_ARG( "rdldbb", &magick_wand_rsrc_zvl_p, &num_cols, &col_space, &treedepth, &dither, &measure_err );

	MW_CHECK_CONSTANT( ColorspaceType, col_space );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL(
		MagickQuantizeImage(
			magick_wand,
			(unsigned long) num_cols,
			(ColorspaceType) col_space,
			(unsigned long) treedepth,
			MW_MK_MGCK_BOOL(dither),
			MW_MK_MGCK_BOOL(measure_err)
		)
	);
}
/* }}} */

/* {{{ proto bool MagickQuantizeImages( MagickWand magick_wand, float number_colors, int colorspace_type, float treedepth, bool dither, bool measure_error )
*/
PHP_FUNCTION( magickquantizeimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double num_cols, treedepth;
	long col_space;
	zend_bool dither, measure_err;

	MW_GET_6_ARG( "rdldbb", &magick_wand_rsrc_zvl_p, &num_cols, &col_space, &treedepth, &dither, &measure_err );

	MW_CHECK_CONSTANT( ColorspaceType, col_space );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL(
		MagickQuantizeImages(
			magick_wand,
			(unsigned long) num_cols,
			(ColorspaceType) col_space,
			(unsigned long) treedepth,
			MW_MK_MGCK_BOOL(dither),
			MW_MK_MGCK_BOOL(measure_err)
		)
	);
}
/* }}} */

/* {{{ proto string MagickQueryConfigureOption( string option )
*/
PHP_FUNCTION( magickqueryconfigureoption )
{
	MW_PRINT_DEBUG_INFO

	char *option, *value;
	int option_len;

	MW_GET_2_ARG( "s", &option, &option_len );

	MW_CHECK_PARAM_STR_LEN( option_len );

	value = (char *) MagickQueryConfigureOption( option );

	if ( value == (char *) NULL || *value == '\0' ) {
		RETURN_MW_EMPTY_STRING();
	}

	RETVAL_STRING( value, 1 );
	MW_FREE_MAGICK_MEM( char *, value );
}
/* }}} */

/* {{{ proto array MagickQueryConfigureOptions( string pattern )
*/
PHP_FUNCTION( magickqueryconfigureoptions )
{
	MW_PRINT_DEBUG_INFO

	MW_RETURN_QUERY_STRING_ARR( MagickQueryConfigureOptions );
}
/* }}} */

/* {{{ proto array MagickQueryFontMetrics( MagickWand magick_wand, DrawingWand draw_wand, string txt [, bool multiline] )
*/
PHP_FUNCTION( magickqueryfontmetrics )
{
	MW_PRINT_DEBUG_INFO

	MW_INIT_FONT_METRIC_ARRAY( font_metric_arr );

	if ( font_metric_arr == (double *) NULL ) {
		RETURN_FALSE;
	}
	else {
		int i;

		array_init( return_value );

		for ( i = 0; i < 13; i++ ) {
			if ( add_next_index_double( return_value, font_metric_arr[i] ) == FAILURE ) {
				MW_SPIT_FATAL_ERR( "error adding a value to the return array" );
				break;
			}
		}
		font_metric_arr = MagickRelinquishMemory( font_metric_arr );
	}
}
/* }}} */

/* {{{ proto float MagickGetCharWidth( MagickWand magick_wand, DrawingWand draw_wand, string txt [, bool multiline] )
*/
PHP_FUNCTION( magickgetcharwidth )
{
	MW_PRINT_DEBUG_INFO

	MW_RETURN_FONT_METRIC( 0 );
}
/* }}} */

/* {{{ proto float MagickGetCharHeight( MagickWand magick_wand, DrawingWand draw_wand, string txt [, bool multiline] )
*/
PHP_FUNCTION( magickgetcharheight )
{
	MW_PRINT_DEBUG_INFO

	MW_RETURN_FONT_METRIC( 1 );
}
/* }}} */

/* {{{ proto float MagickGetTextAscent( MagickWand magick_wand, DrawingWand draw_wand, string txt [, bool multiline] )
*/
PHP_FUNCTION( magickgettextascent )
{
	MW_PRINT_DEBUG_INFO

	MW_RETURN_FONT_METRIC( 2 );
}
/* }}} */

/* {{{ proto float MagickGetTextDescent( MagickWand magick_wand, DrawingWand draw_wand, string txt [, bool multiline] )
*/
PHP_FUNCTION( magickgettextdescent )
{
	MW_PRINT_DEBUG_INFO

	MW_RETURN_FONT_METRIC( 3 );
}
/* }}} */

/* {{{ proto float MagickGetStringWidth( MagickWand magick_wand, DrawingWand draw_wand, string txt [, bool multiline] )
*/
PHP_FUNCTION( magickgetstringwidth )
{
	MW_PRINT_DEBUG_INFO

	MW_RETURN_FONT_METRIC( 4 );
}
/* }}} */

/* {{{ proto float MagickGetStringHeight( MagickWand magick_wand, DrawingWand draw_wand, string txt [, bool multiline] )
*/
PHP_FUNCTION( magickgetstringheight )
{
	MW_PRINT_DEBUG_INFO

	MW_RETURN_FONT_METRIC( 5 );
}
/* }}} */

/* {{{ proto float MagickGetMaxTextAdvance( MagickWand magick_wand, DrawingWand draw_wand, string txt [, bool multiline] )
*/
PHP_FUNCTION( magickgetmaxtextadvance )
{
	MW_PRINT_DEBUG_INFO

	MW_RETURN_FONT_METRIC( 6 );
}
/* }}} */

/* {{{ proto array MagickQueryFonts( string pattern )
*/
PHP_FUNCTION( magickqueryfonts )
{
	MW_PRINT_DEBUG_INFO

	MW_RETURN_QUERY_STRING_ARR( MagickQueryFonts );
}
/* }}} */

/* {{{ proto array MagickQueryFormats( string pattern )
*/
PHP_FUNCTION( magickqueryformats )
{
	MW_PRINT_DEBUG_INFO

	MW_RETURN_QUERY_STRING_ARR( MagickQueryFormats );
}
/* }}} */

/* {{{ proto bool MagickRadialBlurImage( MagickWand magick_wand, float angle [, int channel_type] )
*/
PHP_FUNCTION( magickradialblurimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double angle;
	long channel = -1;

	MW_GET_3_ARG( "rd|l", &magick_wand_rsrc_zvl_p, &angle, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickRadialBlurImage( magick_wand, angle ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickRadialBlurImageChannel( magick_wand, (ChannelType) channel, angle ) );
	}
}
/* }}} */

/* {{{ proto bool MagickRaiseImage( MagickWand magick_wand, int width, int height, int x, int y [, bool raise] )
*/
PHP_FUNCTION( magickraiseimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double width, height;
	long x, y;
	zend_bool do_raise = TRUE;

	MW_GET_6_ARG( "rddll|b", &magick_wand_rsrc_zvl_p, &width, &height, &x, &y, &do_raise );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickRaiseImage(	magick_wand,
												(unsigned long) width, (unsigned long) height,
												x, y,
												MW_MK_MGCK_BOOL(do_raise) ) );
}
/* }}} */

/* {{{ proto bool MagickReadImage( MagickWand magick_wand, string filename )
*/
PHP_FUNCTION( magickreadimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *format_or_file;
	int format_or_file_len;

	MW_GET_3_ARG( "rs", &magick_wand_rsrc_zvl_p, &format_or_file, &format_or_file_len );

	MW_CHECK_PARAM_STR_LEN( format_or_file_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_DEBUG_NOTICE_EX( "trying to read \"%s\"", format_or_file );

	if ( MW_read_image( magick_wand, format_or_file ) == MagickFalse ) {
		return;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool MagickReadImageBlob( MagickWand magick_wand, string blob )
*/
PHP_FUNCTION( magickreadimageblob )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *blob;
	int blob_len;
	unsigned long orig_img_idx;

	MW_GET_3_ARG( "rs", &magick_wand_rsrc_zvl_p, &blob, &blob_len );

	MW_CHECK_PARAM_STR_LEN( blob_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	orig_img_idx = (unsigned long) MagickGetNumberImages( magick_wand );

	if( MagickReadImageBlob( magick_wand, (void *) blob, (size_t) blob_len ) == MagickTrue ) {

		if ( MagickSetIteratorIndex( magick_wand, (long) orig_img_idx ) == MagickTrue ) {
			while ( MagickSetImageFilename( magick_wand, (char *) NULL ), MagickNextImage( magick_wand ) == MagickTrue )
				;
		}
		MagickClearException( magick_wand );
		MagickResetIterator( magick_wand );
		RETVAL_TRUE;
	}
	else {
		/* C API cannot read the image(s) from the BLOB: output error, with reason */
		MW_API_FUNC_FAIL_CHECK_WAND_ERROR( magick_wand, MagickWand, "unable to read the supplied BLOB argument" );
	}
}
/* }}} */

/* {{{ proto bool MagickReadImageFile( MagickWand magick_wand, php_stream handle )
*/
PHP_FUNCTION( magickreadimagefile )
{
	MagickWand *magick_wand;
	zval *zstream;
	php_stream *stream;
	FILE *fp;
	zval *magick_wand_rsrc_zvl_p;
	
	MW_GET_2_ARG( "rr", &magick_wand_rsrc_zvl_p, &zstream );
	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	php_stream_from_zval(stream, &zstream);
	
	if (php_stream_can_cast(stream, PHP_STREAM_AS_STDIO | PHP_STREAM_CAST_INTERNAL) == FAILURE) {
		return;
	}

	if (php_stream_cast(stream, PHP_STREAM_AS_STDIO | PHP_STREAM_CAST_INTERNAL, (void*)&fp, 0) == FAILURE) {
		return;
	}
	
	if (MagickReadImageFile(magick_wand, fp)) {
		unsigned long orig_img_idx = (unsigned long) MagickGetNumberImages( magick_wand );
		
		if ( MagickSetIteratorIndex( magick_wand, (long) orig_img_idx ) == MagickTrue ) {
			while ( MagickSetImageFilename( magick_wand, (char *) NULL ), MagickNextImage( magick_wand ) == MagickTrue )
				;
		}
		MagickClearException( magick_wand );
		MagickResetIterator( magick_wand );
		RETVAL_TRUE;
	} else {
		MW_API_FUNC_FAIL_CHECK_WAND_ERROR( magick_wand, MagickWand, "unable to read the image from the filehandle" );
	}
}
/* }}} */

/* {{{ proto bool MagickReadImages( MagickWand mgck_wand, array img_filenames_array )
*/
PHP_FUNCTION( magickreadimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p, *zvl_arr, **zvl_pp_element;
	int num_files, i;
	HashPosition pos;
	char *format_or_file;

	MW_GET_2_ARG( "ra", &magick_wand_rsrc_zvl_p, &zvl_arr );

	num_files = zend_hash_num_elements( Z_ARRVAL_P( zvl_arr ) );

	if ( num_files < 1 ) {
		zend_error( MW_E_ERROR, "%s(): function requires an array containing at least 1 image filename",
								get_active_function_name(TSRMLS_C) );
		return;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	i = 0;

	MW_ITERATE_OVER_PHP_ARRAY( pos, zvl_arr, zvl_pp_element ) {
		convert_to_string_ex( zvl_pp_element );

		if ( Z_STRLEN_PP( zvl_pp_element ) < 1 ) {
			zend_error( MW_E_ERROR, "%s(): image filename at index %d of argument array was empty",
									get_active_function_name(TSRMLS_C), i );
			return;
		}

		format_or_file = Z_STRVAL_PP( zvl_pp_element );

		if ( MW_read_image( magick_wand, format_or_file ) == MagickFalse ) {
			return;
		}

		i++;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool MagickRecolorImage( MagickWand magick_wand, array kernel_array )
*/
PHP_FUNCTION( magickrecolorimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p, *zvl_arr, **zvl_pp_element;
	unsigned long order, i = 0;
	double num_elements, *color_matrix_arr;
	HashPosition pos;

	MW_GET_2_ARG( "ra", &magick_wand_rsrc_zvl_p, &zvl_arr );

	num_elements = (double) zend_hash_num_elements( Z_ARRVAL_P( zvl_arr ) );

	if ( num_elements < 1 ) {
		MW_SPIT_FATAL_ERR( "the array parameter was empty" );
		return;
	}

	order = (unsigned long) sqrt( num_elements );

	if ( pow( (double) order, 2 ) != num_elements ) {
		MW_SPIT_FATAL_ERR( "array parameter length was not square; array must contain a square number amount of doubles" );
		return;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_ARR_ECALLOC( double, color_matrix_arr, num_elements );

	MW_ITERATE_OVER_PHP_ARRAY( pos, zvl_arr, zvl_pp_element ) {
		convert_to_double_ex( zvl_pp_element );

		color_matrix_arr[i++] = Z_DVAL_PP( zvl_pp_element );
	}

		MW_BOOL_FUNC_RETVAL_BOOL( MagickRecolorImage( magick_wand, order, color_matrix_arr ) );

	efree( color_matrix_arr );
}
/* }}} */

/* {{{ proto bool MagickReduceNoiseImage( MagickWand magick_wand, float radius )
*/
PHP_FUNCTION( magickreducenoiseimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_DOUBLE_RET_BOOL( MagickReduceNoiseImage );
}
/* }}} */

/* {{{ proto bool MagickRemoveImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickremoveimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( MagickWand, MagickRemoveImage );
}
/* }}} */

/* {{{ proto string MagickRemoveImageProfile( MagickWand magick_wand, string name )
*/
PHP_FUNCTION( magickremoveimageprofile )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *name;
	int name_len;
	size_t profile_len = 0;

	MW_GET_3_ARG( "rs", &magick_wand_rsrc_zvl_p, &name, &name_len );

	MW_CHECK_PARAM_STR_LEN( name_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_FUNC_RETVAL_STRING_L( magick_wand, MagickWand, MagickRemoveImageProfile( magick_wand, name, &profile_len ), profile_len );
/*
	unsigned char *profile;

	profile = (unsigned char *) MagickRemoveImageProfile( magick_wand, name, &length );
	MW_FREE_MAGICK_MEM( unsigned char *, profile  );

	if ( MagickGetExceptionType(magick_wand) == UndefinedException ) {
		RETURN_TRUE;
	}
	else {
		RETURN_FALSE;
	}
*/
}
/* }}} */

/* {{{ proto bool MagickRemoveImageProfiles( MagickWand magick_wand )
*/
PHP_FUNCTION( magickremoveimageprofiles )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickProfileImage( magick_wand, "*", (void *) NULL, 0 ) );
}
/* }}} */

/* {{{ proto bool MagickResetImagePage( MagickWand magick_wand [, string page] )
*/
PHP_FUNCTION( magickresetimagepage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *page;
	int page_len = 0;

	MW_GET_3_ARG( "r|s", &magick_wand_rsrc_zvl_p, &page, &page_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

/* This function can be used to REMOVE an image page, by specifying NULL or "" as the page argument
*/
	if ( page_len < 1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickResetImagePage( magick_wand, (char *) NULL ) );
	}
	else {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickResetImagePage( magick_wand, page ) );
	}
}
/* }}} */

/* {{{ proto void MagickResetIterator( MagickWand magick_wand )
*/
PHP_FUNCTION( magickresetiterator )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MagickResetIterator( magick_wand );
}
/* }}} */

/* {{{ proto bool MagickResampleImage( MagickWand mgck_wand, float x_resolution, float y_resolution, int filter_type, float blur )
*/
PHP_FUNCTION( magickresampleimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x_resolution, y_resolution, blur;
	long filter;

	MW_GET_5_ARG( "rddld", &magick_wand_rsrc_zvl_p, &x_resolution, &y_resolution, &filter, &blur );

	MW_CHECK_CONSTANT( FilterTypes, filter );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickResampleImage( magick_wand, x_resolution, y_resolution, (FilterTypes) filter, blur ) );
}
/* }}} */

/* {{{ proto bool MagickResizeImage( MagickWand mgck_wand, float columns, float rows, int filter_type, float blur )
*/
PHP_FUNCTION( magickresizeimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long filter;
	double columns, rows, blur;

	MW_GET_5_ARG( "rddld", &magick_wand_rsrc_zvl_p, &columns, &rows, &filter, &blur );

	MW_CHECK_CONSTANT( FilterTypes, filter );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickResizeImage( magick_wand, (unsigned long) columns, (unsigned long) rows, (FilterTypes) filter, blur ) );
}
/* }}} */

/* {{{ proto bool MagickRollImage( MagickWand magick_wand, int x_offset, int y_offset )
*/
PHP_FUNCTION( magickrollimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long x_offset, y_offset;

	MW_GET_3_ARG( "rll", &magick_wand_rsrc_zvl_p, &x_offset, &y_offset );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickRollImage( magick_wand, x_offset, y_offset ) );
}
/* }}} */

/* {{{ proto bool MagickRotateImage( MagickWand magick_wand, mixed background_pixel_wand, float degrees )
*/
PHP_FUNCTION( magickrotateimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *background_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;
	double degrees;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 3),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, a background color PixelWand resource (or "  \
							"ImageMagick color string), and the desired degrees of rotation" );

	convert_to_double_ex( zvl_pp_args_arr[2] );
	degrees =  Z_DVAL_PP( zvl_pp_args_arr[2] );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, background_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickRotateImage( magick_wand, background_pixel_wand, degrees ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		background_pixel_wand = DestroyPixelWand( background_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickSampleImage( MagickWand magick_wand, float columns, float rows )
*/
PHP_FUNCTION( magicksampleimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double columns, rows;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &columns, &rows );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSampleImage( magick_wand, (unsigned long) columns, (unsigned long) rows ) );
}
/* }}} */

/* {{{ proto bool MagickScaleImage( MagickWand magick_wand, float columns, float rows )
*/
PHP_FUNCTION( magickscaleimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double columns, rows;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &columns, &rows );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickScaleImage( magick_wand, (unsigned long) columns, (unsigned long) rows ) );
}
/* }}} */

/* {{{ proto bool MagickSeparateImageChannel( MagickWand magick_wand, int channel_type )
*/
PHP_FUNCTION( magickseparateimagechannel )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( ChannelType, MagickSeparateImageChannel );
}
/* }}} */

/* {{{ proto bool MagickSepiaToneImage( MagickWand magick_wand, float threshold )
*/
PHP_FUNCTION( magicksepiatoneimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double threshold;

	MW_GET_2_ARG( "rd", &magick_wand_rsrc_zvl_p, &threshold );

	if ( threshold < 0.0 || threshold > MW_QuantumRange ) {
		zend_error( MW_E_ERROR,
					"%s(): value of threshold argument (%0.0f) was invalid. "
					"Threshold value must match \"0 <= threshold <= %0.0f\"",
					get_active_function_name( TSRMLS_C ), threshold, MW_QuantumRange );
		return;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSepiaToneImage( magick_wand, threshold ) );
}
/* }}} */

/* {{{ proto bool MagickSetBackgroundColor( MagickWand magick_wand, mixed background_pixel_wand )
*/
PHP_FUNCTION( magicksetbackgroundcolor )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *background_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 2),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource and a background color PixelWand resource "  \
							"(or ImageMagick color string)" );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, background_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetBackgroundColor( magick_wand, background_pixel_wand ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		background_pixel_wand = DestroyPixelWand( background_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickSetCompression( MagickWand magick_wand, int compression_type )
*/
PHP_FUNCTION( magicksetcompression )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( CompressionType, MagickSetCompression );
}
/* }}} */

/* {{{ proto bool MagickSetCompressionQuality( MagickWand magick_wand, float quality )
*/
PHP_FUNCTION( magicksetcompressionquality )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double quality;

	MW_GET_2_ARG( "rd", &magick_wand_rsrc_zvl_p, &quality );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetCompressionQuality( magick_wand, (unsigned long) quality ) );
}
/* }}} */

/* {{{ proto bool MagickSetFilename( MagickWand magick_wand [, string filename] )
*/
PHP_FUNCTION( magicksetfilename )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *filename;
	int filename_len = 0;

	MW_GET_3_ARG( "r|s", &magick_wand_rsrc_zvl_p, &filename, &filename_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

/* The following is commented out as this function can be used to REMOVE an image filename, by
   specifying NULL or "" as the filename argument

	MW_CHECK_PARAM_STR_LEN( filename_len );
*/
	if ( filename_len < 1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickSetFilename( magick_wand, (char *) NULL ) );
	}
	else {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickSetFilename( magick_wand, filename ) );
	}
}
/* }}} */

/* {{{ proto void MagickSetFirstIterator( MagickWand magick_wand )
*/
PHP_FUNCTION( magicksetfirstiterator )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MagickSetFirstIterator( magick_wand );
}
/* }}} */

/* {{{ proto bool MagickSetFormat( MagickWand magick_wand, string format )
*/
PHP_FUNCTION( magicksetformat )
{
	MW_PRINT_DEBUG_INFO

	MW_SET_MAGICK_FORMAT( MagickSetFormat );
}
/* }}} */

/* {{{ proto bool MagickSetImage( MagickWand magick_wand, MagickWand replace_wand )
*/
PHP_FUNCTION( magicksetimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand, *set_wnd;
	zval *magick_wand_rsrc_zvl_p, *set_wnd_rsrc_zvl_p;

	MW_GET_2_ARG( "rr", &magick_wand_rsrc_zvl_p, &set_wnd_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );
	MW_GET_POINTER_FROM_RSRC( MagickWand, set_wnd, &set_wnd_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImage( magick_wand, set_wnd ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageProperty( MagickWand magick_wand, string key, string value )
*/
PHP_FUNCTION( magicksetimageattribute )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *key, *script_value, *value;
	int key_len, value_len;

	MW_GET_5_ARG( "rss", &magick_wand_rsrc_zvl_p, &key, &key_len, &script_value, &value_len );

	MW_CHECK_PARAM_STR_LEN( key_len );

	if ( value_len < 1 ) {
		value = (char *) NULL;
	}
	else {
		value = script_value;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageProperty( magick_wand, key, value ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageAlphaChannel( MagickWand magick_wand, int alpha_type )
*/
PHP_FUNCTION( magicksetimagealphachannel )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( AlphaChannelType, MagickSetImageAlphaChannel );
}
/* }}} */

/* {{{ proto bool MagickSetImageBackgroundColor( MagickWand magick_wand, mixed background_pixel_wand )
*/
PHP_FUNCTION( magicksetimagebackgroundcolor )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *background_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 2),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource and a background color PixelWand resource "  \
							"(or ImageMagick color string)" );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, background_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageBackgroundColor( magick_wand, background_pixel_wand ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		background_pixel_wand = DestroyPixelWand( background_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickSetImageBias( MagickWand magick_wand, float bias )
*/
PHP_FUNCTION( magicksetimagebias )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_DOUBLE_RET_BOOL( MagickSetImageBias );
}
/* }}} */

/* {{{ proto bool MagickSetImageBluePrimary( MagickWand magick_wand, float x, float y )
*/
PHP_FUNCTION( magicksetimageblueprimary )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageBluePrimary( magick_wand, x, y ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageBorderColor( MagickWand magick_wand, mixed border_pixel_wand )
*/
PHP_FUNCTION( magicksetimagebordercolor )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *border_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 2),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource and a border color PixelWand resource "  \
							"(or ImageMagick color string)" );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, border_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageBorderColor( magick_wand, border_pixel_wand ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		border_pixel_wand = DestroyPixelWand( border_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickSetImageClipMask( MagickWand magick_wand, MagickWand clip_wand )
*/
PHP_FUNCTION( magicksetimageclipmask )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand, *clip_wnd;
	zval *magick_wand_rsrc_zvl_p, *clip_wnd_rsrc_zvl_p;

	MW_GET_2_ARG( "rr", &magick_wand_rsrc_zvl_p, &clip_wnd_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );
	MW_GET_POINTER_FROM_RSRC( MagickWand, clip_wnd, &clip_wnd_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageClipMask( magick_wand, clip_wnd ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageColormapColor( MagickWand magick_wand, float index, mixed mapcolor_pixel_wand )
*/
PHP_FUNCTION( magicksetimagecolormapcolor )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *mapcolor_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;
	double index;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 3),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, the image colormap offset, and "  \
							"a map color PixelWand resource (or ImageMagick color string)" );

	convert_to_double_ex( zvl_pp_args_arr[1] );
	index =    Z_DVAL_PP( zvl_pp_args_arr[1] );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 2, 3, mapcolor_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageColormapColor( magick_wand, (unsigned long) index, mapcolor_pixel_wand ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		mapcolor_pixel_wand = DestroyPixelWand( mapcolor_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickSetImageColorspace( MagickWand magick_wand, int colorspace_type )
*/
PHP_FUNCTION( magicksetimagecolorspace )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( ColorspaceType, MagickSetImageColorspace );
}
/* }}} */

/* {{{ proto bool MagickSetImageCompose( MagickWand magick_wand, int composite_operator )
*/
PHP_FUNCTION( magicksetimagecompose )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( CompositeOperator, MagickSetImageCompose );
}
/* }}} */

/* {{{ proto bool MagickSetImageCompression( MagickWand magick_wand, int compression_type )
*/
PHP_FUNCTION( magicksetimagecompression )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( CompressionType, MagickSetImageCompression );
}
/* }}} */

/* {{{ proto bool MagickSetImageCompressionQuality( MagickWand magick_wand, float quality )
*/
PHP_FUNCTION( magicksetimagecompressionquality )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double quality;

	MW_GET_2_ARG( "rd", &magick_wand_rsrc_zvl_p, &quality );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageCompressionQuality( magick_wand, (unsigned long) quality ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageDelay( MagickWand magick_wand, float delay )
*/
PHP_FUNCTION( magicksetimagedelay )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double delay;

	MW_GET_2_ARG( "rd", &magick_wand_rsrc_zvl_p, &delay );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageDelay( magick_wand, (unsigned long) delay ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageDepth( MagickWand magick_wand, int depth [, int channel_type]] )
*/
PHP_FUNCTION( magicksetimagedepth )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long depth, channel = -1;

	MW_GET_3_ARG( "rl|l", &magick_wand_rsrc_zvl_p, &depth, &channel );

	if ( !( depth == 8 || depth == 16 || depth == 32 ) ) {
		zend_error( MW_E_ERROR, "%s(): image channel depth argument cannot be %ld; it must be either 8, 16, or 32",
								get_active_function_name(TSRMLS_C), depth );
		RETURN_FALSE;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageDepth( magick_wand, (unsigned long) depth ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageChannelDepth( magick_wand, (ChannelType) channel, (unsigned long) depth ) );
	}
}
/* }}} */

/* {{{ proto bool MagickSetImageDispose( MagickWand magick_wand, int dispose_type )
*/
PHP_FUNCTION( magicksetimagedispose )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( DisposeType, MagickSetImageDispose );
}
/* }}} */

/* {{{ proto bool MagickSetImageEndian( MagickWand magick_wand, int endian_type ) 
 *    */ 
PHP_FUNCTION( magicksetimageendian ) 
{ 
   MW_PRINT_DEBUG_INFO 

   MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( EndianType, MagickSetImageEndian ); 
} 
/* }}} */

/* {{{ proto bool MagickSetImageExtent( MagickWand magick_wand, int columns, int rows )
*/
PHP_FUNCTION( magicksetimageextent )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long columns, rows;

	MW_GET_3_ARG( "rll", &magick_wand_rsrc_zvl_p, &columns, &rows );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageExtent( magick_wand, (unsigned long) columns, (unsigned long) rows ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageFilename( MagickWand magick_wand [, string filename] )
*/
PHP_FUNCTION( magicksetimagefilename )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *filename;
	int filename_len = 0;

	MW_GET_3_ARG( "r|s", &magick_wand_rsrc_zvl_p, &filename, &filename_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

/* This function can be used to REMOVE an image filename, by specifying NULL or "" as the filename argument
*/
	if ( filename_len < 1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageFilename( magick_wand, (char *) NULL ) );
	}
	else {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageFilename( magick_wand, filename ) );
	}
}
/* }}} */

/* {{{ proto bool MagickSetImageFormat( MagickWand magick_wand, string format )
*/
PHP_FUNCTION( magicksetimageformat )
{
	MW_PRINT_DEBUG_INFO

	MW_SET_MAGICK_FORMAT( MagickSetImageFormat );
}
/* }}} */

/* {{{ proto bool MagickSetImageGamma( MagickWand magick_wand, float gamma )
*/
PHP_FUNCTION( magicksetimagegamma )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_DOUBLE_RET_BOOL( MagickSetImageGamma );
}
/* }}} */

/* {{{ proto bool MagickSetImageGreenPrimary( MagickWand magick_wand, float x, float y )
*/
PHP_FUNCTION( magicksetimagegreenprimary )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageGreenPrimary( magick_wand, x, y ) );
}
/* }}} */

/* {{{ proto bool MagickSetIteratorIndex( MagickWand magick_wand, int index )
*/
PHP_FUNCTION( magicksetimageindex )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long index;

	MW_GET_2_ARG( "rl", &magick_wand_rsrc_zvl_p, &index );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetIteratorIndex( magick_wand, index ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageInterlaceScheme( MagickWand magick_wand, int interlace_type )
*/
PHP_FUNCTION( magicksetimageinterlacescheme )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( InterlaceType, MagickSetImageInterlaceScheme );
}
/* }}} */

/* {{{ proto bool MagickSetImageIterations( MagickWand magick_wand, float iterations )
*/
PHP_FUNCTION( magicksetimageiterations )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double iterations;

	MW_GET_2_ARG( "rd", &magick_wand_rsrc_zvl_p, &iterations );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageIterations( magick_wand, (unsigned long) iterations ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageMatteColor( MagickWand magick_wand, mixed matte_pixel_wand )
*/
PHP_FUNCTION( magicksetimagemattecolor )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *matte_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 2),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource and a matte color PixelWand resource "  \
							"(or ImageMagick color string)" );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, matte_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageMatteColor( magick_wand, matte_pixel_wand ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		matte_pixel_wand = DestroyPixelWand( matte_pixel_wand );
	}
}
/* }}} */


/* {{{ proto bool MagickSetImageOpacity( MagickWand magick_wand, float opacity )
*/
PHP_FUNCTION( magicksetimageopacity )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_DOUBLE_RET_BOOL( MagickSetImageOpacity );
}
/* }}} */

/* {{{ proto bool MagickSetImageOption( MagickWand magick_wand, string format, string key, string value )
*/
PHP_FUNCTION( magicksetimageoption )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *format, *key, *value;
	int format_len, key_len, value_len;

	MW_GET_7_ARG( "rsss", &magick_wand_rsrc_zvl_p, &format, &format_len, &key, &key_len, &value, &value_len );

	MW_CHECK_PARAM_STR_LEN_EX( (format_len == 0 || key_len == 0 || value_len == 0) );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageOption( magick_wand, format, key, value ) );
}
/* }}} */

/* {{{ proto bool MagickSetImagePage( MagickWand magick_wand, int width, int height, int x, int y )
*/
PHP_FUNCTION( magicksetimagepage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double width, height;
	long x, y;

	MW_GET_5_ARG( "rddll", &magick_wand_rsrc_zvl_p, &width, &height, &x, &y );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImagePage( magick_wand, (unsigned long) width, (unsigned long) height, x, y ) );
}
/* }}} */

/* {{{ proto bool MagickImportImagePixels( MagickWand magick_wand, int x_offset, int y_offset, float columns, float rows, string map, int storage_type, array pixel_array )
*/
PHP_FUNCTION( magicksetimagepixels )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p, *zvl_arr, **zvl_pp_element;
	long x_offset, y_offset, php_storage;
	double columns, rows;
	char *map, *c;
	int map_len, pixel_array_length, i;
	StorageType storage;
	HashPosition pos;

	MW_GET_9_ARG( "rllddsla",
				  &magick_wand_rsrc_zvl_p,
				  &x_offset, &y_offset,
				  &columns, &rows,
				  &map, &map_len,
				  &php_storage,
				  &zvl_arr );

	MW_CHECK_PARAM_STR_LEN( map_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	for ( i = 0; i < map_len; i++ ) {
		c = &(map[i]);

		if ( !( *c == 'A' || *c == 'a' ||
				*c == 'B' || *c == 'b' ||
				*c == 'C' || *c == 'c' ||
				*c == 'G' || *c == 'g' ||
				*c == 'I' || *c == 'i' ||
				*c == 'K' || *c == 'k' ||
				*c == 'M' || *c == 'm' ||
				*c == 'O' || *c == 'o' ||
				*c == 'P' || *c == 'p' ||
				*c == 'R' || *c == 'r' ||
				*c == 'Y' || *c == 'y' ) ) {
			MW_SPIT_FATAL_ERR( "map parameter string can only contain the letters A B C G I K M O P R Y; see docs for details" );
			return;
		}
	}

	pixel_array_length = zend_hash_num_elements( Z_ARRVAL_P( zvl_arr ) );

	if ( pixel_array_length == 0 ) {
		MW_SPIT_FATAL_ERR( "pixel array parameter was empty" );
		return;
	}

	i = (int) ( columns * rows * map_len );

	if ( pixel_array_length != i ) {
		zend_error( MW_E_ERROR, "%s(): actual size of pixel array (%d) does not match expected size (%u)",
								get_active_function_name( TSRMLS_C ), pixel_array_length, i );
		return;
	}

	storage = (StorageType) php_storage;

	i = 0;

#define PRV_SET_PIXELS_ARR_RET_BOOL_2( type )  \
{   type *pixel_array;  \
	MW_ARR_ECALLOC( type, pixel_array, pixel_array_length );  \
	MW_ITERATE_OVER_PHP_ARRAY( pos, zvl_arr, zvl_pp_element )  \
	{  \
		convert_to_double_ex( zvl_pp_element );  \
		pixel_array[ i++ ] = (type) Z_DVAL_PP( zvl_pp_element );  \
	}  \
	MW_BOOL_FUNC_RETVAL_BOOL(  \
		MagickImportImagePixels( magick_wand,  \
							  x_offset, y_offset,  \
							  (unsigned long) columns, (unsigned long) rows,  \
							  map,  \
							  storage,  \
							  (void *) pixel_array ) );  \
	efree( pixel_array );  \
}

	switch ( storage ) {
		 case CharPixel		: PRV_SET_PIXELS_ARR_RET_BOOL_2( unsigned char  );  break;
		 case ShortPixel	: PRV_SET_PIXELS_ARR_RET_BOOL_2( unsigned short );  break;
		 case IntegerPixel	: PRV_SET_PIXELS_ARR_RET_BOOL_2( unsigned int   );  break;
		 case LongPixel		: PRV_SET_PIXELS_ARR_RET_BOOL_2( unsigned long  );  break;
		 case FloatPixel	: PRV_SET_PIXELS_ARR_RET_BOOL_2( float          );  break;
		 case DoublePixel	: PRV_SET_PIXELS_ARR_RET_BOOL_2( double         );  break;

		 default : MW_SPIT_FATAL_ERR( "Invalid StorageType argument supplied to function" );
				   return;
	}
}
/* }}} */

/* {{{ proto bool MagickSetImageProfile( MagickWand magick_wand, string name, string profile )
*/
PHP_FUNCTION( magicksetimageprofile )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *name, *profile;
	int name_len, profile_len;

	MW_GET_5_ARG( "rss", &magick_wand_rsrc_zvl_p, &name, &name_len, &profile, &profile_len );

	MW_CHECK_PARAM_STR_LEN_EX( (name_len == 0 || profile_len == 0) );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageProfile( magick_wand, name, (void *) profile, (size_t) profile_len ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageProperty( MagickWand magick_wand, string key, string value )
*/
PHP_FUNCTION( magicksetimageproperty )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *key, *script_value, *value;
	int key_len, value_len;

	MW_GET_5_ARG( "rss", &magick_wand_rsrc_zvl_p, &key, &key_len, &script_value, &value_len );

	MW_CHECK_PARAM_STR_LEN( key_len );

	if ( value_len < 1 ) {
		value = (char *) NULL;
	}
	else {
		value = script_value;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageProperty( magick_wand, key, value ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageRedPrimary( MagickWand magick_wand, float x, float y )
*/
PHP_FUNCTION( magicksetimageredprimary )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageRedPrimary( magick_wand, x, y ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageRenderingIntent( MagickWand magick_wand, int rendering_intent )
*/
PHP_FUNCTION( magicksetimagerenderingintent )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( RenderingIntent, MagickSetImageRenderingIntent );
}
/* }}} */

/* {{{ proto bool MagickSetImageResolution( MagickWand magick_wand, float x_resolution, float y_resolution )
*/
PHP_FUNCTION( magicksetimageresolution )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x_resolution, y_resolution;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &x_resolution, &y_resolution );

	if ( x_resolution <= 0.0 || y_resolution <= 0.0 ) {
		MW_SPIT_FATAL_ERR( "this function does not accept arguments with values less than or equal to 0" );
		return;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageResolution( magick_wand, x_resolution, y_resolution ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageScene( MagickWand magick_wand, float scene )
*/
PHP_FUNCTION( magicksetimagescene )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double scene;

	MW_GET_2_ARG( "rd", &magick_wand_rsrc_zvl_p, &scene );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageScene( magick_wand, (unsigned long) scene ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageTicksPerSecond( MagickWand magick_wand, float ticks_per_second )
*/
PHP_FUNCTION( magicksetimagetickspersecond )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double ticks_per_second;

	MW_GET_2_ARG( "rd", &magick_wand_rsrc_zvl_p, &ticks_per_second );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageTicksPerSecond( magick_wand, (unsigned long) ticks_per_second ) );
}
/* }}} */

/* {{{ proto bool MagickSetImageType( MagickWand magick_wand, int image_type )
*/
PHP_FUNCTION( magicksetimagetype )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( ImageType, MagickSetImageType );
}
/* }}} */

/* {{{ proto bool MagickSetImageUnits( MagickWand magick_wand, int resolution_type )
*/
PHP_FUNCTION( magicksetimageunits )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( ResolutionType, MagickSetImageUnits );
}
/* }}} */

/* {{{ proto bool MagickSetImageVirtualPixelMethod( MagickWand magick_wand, int virtual_pixel_method )
*/
PHP_FUNCTION( magicksetimagevirtualpixelmethod )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( VirtualPixelMethod, MagickSetImageVirtualPixelMethod );
}
/* }}} */

/* {{{ proto bool MagickSetImageWhitePoint( MagickWand magick_wand, float x, float y )
*/
PHP_FUNCTION( magicksetimagewhitepoint )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x, y;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &x, &y );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetImageWhitePoint( magick_wand, x, y ) );
}
/* }}} */

/* {{{ proto bool MagickSetInterlaceScheme( MagickWand magick_wand, InterlaceType interlace_type )
*/
PHP_FUNCTION( magicksetinterlacescheme )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( InterlaceType, MagickSetInterlaceScheme );
}
/* }}} */

/* {{{ proto void MagickSetLastIterator( MagickWand magick_wand )
*/
PHP_FUNCTION( magicksetlastiterator )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MagickSetLastIterator( magick_wand );
}
/* }}} */

/* {{{ proto bool MagickSetOption( MagickWand magick_wand, string key, string value )
*/
PHP_FUNCTION( magicksetoption )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *key, *script_value, *value;
	int key_len, value_len;

	MW_GET_5_ARG( "rss", &magick_wand_rsrc_zvl_p, &key, &key_len, &script_value, &value_len );

	MW_CHECK_PARAM_STR_LEN( key_len );

	if ( value_len < 1 ) {
		value = (char *) NULL;
	}
	else {
		value = script_value;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetOption( magick_wand, key, value ) );
}
/* }}} */

/* {{{ proto bool MagickSetPage( MagickWand magick_wand, int width, int height, int x, int y )
*/
PHP_FUNCTION( magicksetpage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double width, height;
	long x, y;

	MW_GET_5_ARG( "rddll", &magick_wand_rsrc_zvl_p, &width, &height, &x, &y );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetPage( magick_wand, (unsigned long) width, (unsigned long) height, x, y ) );
}
/* }}} */

/* {{{ proto bool MagickSetPassphrase( MagickWand magick_wand, string passphrase )
*/
PHP_FUNCTION( magicksetpassphrase )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_AND_STRING_RETVAL_FUNC_BOOL( MagickWand, MagickSetPassphrase );
}
/* }}} */

/* {{{ proto bool MagickSetResolution( MagickWand magick_wand, float x_resolution, float y_resolution )
*/
PHP_FUNCTION( magicksetresolution )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double x_resolution, y_resolution;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &x_resolution, &y_resolution );

	if ( x_resolution <= 0.0 || y_resolution <= 0.0 ) {
		MW_SPIT_FATAL_ERR( "this function does not accept arguments with values less than or equal to 0" );
		return;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetResolution( magick_wand, x_resolution, y_resolution ) );
}
/* }}} */

/* {{{ proto bool MagickSetResourceLimit( int resource_type, float limit )
*/
PHP_FUNCTION( magicksetresourcelimit )
{
	MW_PRINT_DEBUG_INFO

	long rsrc_type;
	double limit;

	MW_GET_2_ARG( "ld", &rsrc_type, &limit );

	MW_CHECK_CONSTANT( ResourceType, rsrc_type );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetResourceLimit( (ResourceType) rsrc_type, (unsigned long) limit ) );
}
/* }}} */

/* {{{ proto bool MagickSetSamplingFactors( MagickWand magick_wand, float number_factors, array sampling_factors )
*/
PHP_FUNCTION( magicksetsamplingfactors )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p, *zvl_arr, **zvl_pp_element;
	int number_factors, i = 0;
	double *sampling_factors;
	HashPosition pos;

	MW_GET_2_ARG( "ra", &magick_wand_rsrc_zvl_p, &zvl_arr );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	number_factors = zend_hash_num_elements( Z_ARRVAL_P( zvl_arr ) );

	if ( number_factors < 1 ) {
		RETURN_TRUE;
	}

	MW_ARR_ECALLOC( double, sampling_factors, number_factors );

	MW_ITERATE_OVER_PHP_ARRAY( pos, zvl_arr, zvl_pp_element ) {
		convert_to_double_ex( zvl_pp_element );

		sampling_factors[i++] = Z_DVAL_PP( zvl_pp_element );
	}

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetSamplingFactors( magick_wand, (unsigned long) number_factors, sampling_factors ) );

	efree( sampling_factors );
}
/* }}} */

/* {{{ proto bool MagickSetSize( MagickWand magick_wand, int columns, int rows )
*/
PHP_FUNCTION( magicksetsize )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long columns, rows;

	MW_GET_3_ARG( "rll", &magick_wand_rsrc_zvl_p, &columns, &rows );

	if ( columns < 1 || rows < 1 ) {
		MW_SPIT_FATAL_ERR( "this function does not accept arguments with values less than 1" );
		return;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSetSize( magick_wand, (unsigned long) columns, (unsigned long) rows ) );
}
/* }}} */

/* {{{ proto bool MagickSetType( MagickWand magick_wand, int image_type )
*/
PHP_FUNCTION( magicksettype )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_ENUM_RET_BOOL( ImageType, MagickSetType );
}
/* }}} */

/* {{{ proto bool MagickShadowImage( MagickWand magick_wand, float opacity, float sigma, int x_offset, int y_offset )
*/
PHP_FUNCTION( magickshadowimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double opacity, sigma;
	long x_offset, y_offset;

	MW_GET_5_ARG( "rddll", &magick_wand_rsrc_zvl_p, &opacity, &sigma, &x_offset, &y_offset );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( MagickShadowImage( magick_wand, opacity, sigma, x_offset, y_offset ) == MagickTrue ) {
		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool MagickSharpenImage( MagickWand magick_wand, float radius, float sigma [, int channel_type] )
*/
PHP_FUNCTION( magicksharpenimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double radius, sigma;
	long channel = -1;

	MW_GET_4_ARG( "rdd|l", &magick_wand_rsrc_zvl_p, &radius, &sigma, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickSharpenImage( magick_wand, radius, sigma ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickSharpenImageChannel( magick_wand, (ChannelType) channel, radius, sigma ) );
	}
}
/* }}} */

/* {{{ proto bool MagickShaveImage( MagickWand magick_wand, float columns, float rows )
*/
PHP_FUNCTION( magickshaveimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long columns, rows;

	MW_GET_3_ARG( "rll", &magick_wand_rsrc_zvl_p, &columns, &rows );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickShaveImage( magick_wand, (unsigned long) columns, (unsigned long) rows ) );
}
/* }}} */

/* {{{ proto bool MagickShearImage( MagickWand magick_wand, mixed background_pixel_wand, float x_shear, float y_shear )
*/
PHP_FUNCTION( magickshearimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *background_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;
	double x_shear, y_shear;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 4),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource, a background color PixelWand resource "  \
							"(or ImageMagick color string), and x and y shear values" );

	convert_to_double_ex( zvl_pp_args_arr[2] );
	x_shear =  Z_DVAL_PP( zvl_pp_args_arr[2] );
	convert_to_double_ex( zvl_pp_args_arr[3] );
	y_shear =  Z_DVAL_PP( zvl_pp_args_arr[3] );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, background_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickShearImage( magick_wand, background_pixel_wand, x_shear, y_shear ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		background_pixel_wand = DestroyPixelWand( background_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickSolarizeImage( MagickWand magick_wand, float threshold )
*/
PHP_FUNCTION( magicksolarizeimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double threshold;

	MW_GET_2_ARG( "rd", &magick_wand_rsrc_zvl_p, &threshold );

	if ( threshold < 0.0 || threshold > MW_QuantumRange ) {
		zend_error( MW_E_ERROR,
					"%s(): value of threshold argument (%0.0f) was invalid. "
					"Threshold value must match \"0 <= threshold <= %0.0f\"",
					get_active_function_name( TSRMLS_C ), threshold, MW_QuantumRange );
		return;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSolarizeImage( magick_wand, threshold ) );
}
/* }}} */

/* {{{ proto bool MagickSpliceImage( MagickWand magick_wand, int width, int height, int x, int y )
*/
PHP_FUNCTION( magickspliceimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long width, height, x, y;

	MW_GET_5_ARG( "rllll", &magick_wand_rsrc_zvl_p, &width, &height, &x, &y );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickSpliceImage( magick_wand, (unsigned long) width, (unsigned long) height, x, y ) );
}
/* }}} */

/* {{{ proto bool MagickSpreadImage( MagickWand magick_wand, float radius )
*/
PHP_FUNCTION( magickspreadimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_DOUBLE_RET_BOOL( MagickSpreadImage );
}
/* }}} */

/* {{{ proto float MagickStatisticImage( MagickWand magick_wand, StatisicType statisic, float width, float height, [, ChannelType channel] )
*/
PHP_FUNCTION( magickstatisticimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long statistic, channel = -1;
  double width,height;
	MagickBooleanType magick_return;

	MW_GET_5_ARG( "rldd|l", &magick_wand_rsrc_zvl_p, &statistic, &width, &height, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_CHECK_CONSTANT( StatisticType, statistic );

	if ( channel == -1 ) {
		magick_return = MagickStatisticImage( magick_wand, (StatisticType) statistic, (size_t) (width+0.5), (size_t) (height+0.5) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		magick_return = (MagickBooleanType) MagickStatisticImageChannel(	magick_wand,
																				(ChannelType) channel, (StatisticType) statistic,
																				 (size_t) (width+0.5), (size_t) (height+0.5) );
	}

	if ( magick_return == MagickTrue ) {
  	RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto MagickWand MagickSteganoImage( MagickWand magick_wand, MagickWand watermark_wand, int offset )
*/
PHP_FUNCTION( magicksteganoimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand, *watermark_wnd;
	zval *magick_wand_rsrc_zvl_p, *watermark_wnd_rsrc_zvl_p;
	long offset;

	MW_GET_3_ARG( "rrl", &magick_wand_rsrc_zvl_p, &watermark_wnd_rsrc_zvl_p, &offset );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, watermark_wnd, &watermark_wnd_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickSteganoImage( magick_wand, watermark_wnd, offset ) );
}
/* }}} */

/* {{{ proto MagickWand MagickStereoImage( MagickWand magick_wand, MagickWand offset_wand )
*/
PHP_FUNCTION( magickstereoimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand, *offset_wnd;
	zval *magick_wand_rsrc_zvl_p, *offset_wnd_rsrc_zvl_p;

	MW_GET_2_ARG( "rr", &magick_wand_rsrc_zvl_p, &offset_wnd_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, offset_wnd, &offset_wnd_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickStereoImage( magick_wand, offset_wnd ) );
}
/* }}} */

/* {{{ proto bool MagickStripImage( MagickWand magick_wand )
*/
PHP_FUNCTION( magickstripimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickStripImage( magick_wand ) );
}
/* }}} */

/* {{{ proto bool MagickSwirlImage( MagickWand magick_wand, float degrees )
*/
PHP_FUNCTION( magickswirlimage )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_MAGICKWAND_SET_DOUBLE_RET_BOOL( MagickSwirlImage );
}
/* }}} */

/* {{{ proto MagickWand MagickTextureImage( MagickWand magick_wand, MagickWand texture_wand )
*/
PHP_FUNCTION( magicktextureimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand, *tex_wnd;
	zval *magick_wand_rsrc_zvl_p, *tex_wnd_rsrc_zvl_p;

	MW_GET_2_ARG( "rr", &magick_wand_rsrc_zvl_p, &tex_wnd_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, tex_wnd, &tex_wnd_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickTextureImage( magick_wand, tex_wnd ) );
}
/* }}} */

/* {{{ proto bool MagickThresholdImage( MagickWand magick_wand, float threshold [, int channel_type] )
*/
PHP_FUNCTION( magickthresholdimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double threshold;
	long channel = -1;

	MW_GET_3_ARG( "rd|l", &magick_wand_rsrc_zvl_p, &threshold, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickThresholdImage( magick_wand, threshold ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickThresholdImageChannel( magick_wand, (ChannelType) channel, threshold ) );
	}
}
/* }}} */

/* {{{ proto bool MagickThumbnailImage( MagickWand magick_wand, float columns, float rows )
*/
PHP_FUNCTION( magickthumbnailimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double columns, rows;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &columns, &rows );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickThumbnailImage( magick_wand, (unsigned long) columns, (unsigned long) rows ) );
}
/* }}} */

/* {{{ proto bool MagickTintImage( MagickWand magick_wand, mixed tint_pixel_wand, mixed opacity_pixel_wand )
*/
PHP_FUNCTION( magicktintimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *tint_pixel_wand, *opacity_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand, is_script_pixel_wand_2;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 3),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource and a tint color PixelWand resource (or ImageMagick color string)" );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, tint_pixel_wand, is_script_pixel_wand );

	if ( Z_TYPE_PP( zvl_pp_args_arr[2] ) == IS_RESOURCE ) {

		if (  (   MW_FETCH_RSRC( PixelWand, opacity_pixel_wand, zvl_pp_args_arr[2] ) == MagickFalse
			   && MW_FETCH_RSRC( PixelIteratorPixelWand, opacity_pixel_wand, zvl_pp_args_arr[2] ) == MagickFalse )
			|| IsPixelWand( opacity_pixel_wand ) == MagickFalse )
		{
			MW_SPIT_FATAL_ERR( "invalid resource type as argument #3; a PixelWand resource is required" );
			efree( zvl_pp_args_arr );
			if ( !is_script_pixel_wand ) {
				tint_pixel_wand = DestroyPixelWand( tint_pixel_wand );
			}
			return;
		}
		is_script_pixel_wand_2 = 1;
	}
	else {
		is_script_pixel_wand_2 = 0;

		opacity_pixel_wand = (PixelWand *) NewPixelWand();

		if ( opacity_pixel_wand == (PixelWand *) NULL ) {
			MW_SPIT_FATAL_ERR( "unable to create necessary PixelWand" );
			efree( zvl_pp_args_arr );
			if ( !is_script_pixel_wand ) {
				tint_pixel_wand = DestroyPixelWand( tint_pixel_wand );
			}
			return;
		}
		convert_to_string_ex( zvl_pp_args_arr[2] );

		if ( Z_STRLEN_PP( zvl_pp_args_arr[2] ) > 0 ) {

			if ( PixelSetColor( opacity_pixel_wand, Z_STRVAL_PP( zvl_pp_args_arr[2] ) ) == MagickFalse ) {

				MW_API_FUNC_FAIL_CHECK_WAND_ERROR(	opacity_pixel_wand, PixelWand,
													"could not set PixelWand to desired fill color"
				);

				opacity_pixel_wand = DestroyPixelWand( opacity_pixel_wand );
				efree( zvl_pp_args_arr );
				if ( !is_script_pixel_wand ) {
					tint_pixel_wand = DestroyPixelWand( tint_pixel_wand );
				}

				return;
			}
		}
	}

	MW_BOOL_FUNC_RETVAL_BOOL( MagickTintImage( magick_wand, tint_pixel_wand, opacity_pixel_wand ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		tint_pixel_wand = DestroyPixelWand( tint_pixel_wand );
	}
	if ( !is_script_pixel_wand_2 ) {
		opacity_pixel_wand = DestroyPixelWand( opacity_pixel_wand );
	}
}
/* }}} */

/* {{{ proto MagickWand MagickTransformImage( MagickWand magick_wand, string crop, string geometry )
*/
PHP_FUNCTION( magicktransformimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *script_crop, *crop, *script_geom, *geom;
	int crop_len, geom_len;

	MW_GET_5_ARG( "rss", &magick_wand_rsrc_zvl_p, &script_crop, &crop_len, &script_geom, &geom_len );

	MW_CHECK_GEOMETRY_STR_LENGTHS( (crop_len == 0 && geom_len == 0) );

	if ( crop_len < 1 ) {
		crop = (char *) NULL;
	}
	else {
		crop = script_crop;
	}

	if ( geom_len < 1 ) {
		geom = (char *) NULL;
	}
	else {
		geom = script_geom;
	}

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( MagickWand, MagickTransformImage( magick_wand, crop, geom ) );
}
/* }}} */

/* {{{ proto bool MagickTrimImage( MagickWand magick_wand, float fuzz )
*/
PHP_FUNCTION( magicktrimimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double fuzz = 0.0;

	MW_GET_2_ARG( "r|d", &magick_wand_rsrc_zvl_p, &fuzz );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickTrimImage( magick_wand, fuzz ) );
}
/* }}} */

/* {{{ proto bool MagickUnsharpMaskImage( MagickWand magick_wand, float radius, float sigma, float amount, float threshold [, int channel_type] )
*/
PHP_FUNCTION( magickunsharpmaskimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double radius, sigma, amount, threshold;
	long channel = -1;

	MW_GET_6_ARG( "rdddd|l", &magick_wand_rsrc_zvl_p, &radius, &sigma, &amount, &threshold, &channel );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	if ( channel == -1 ) {
		MW_BOOL_FUNC_RETVAL_BOOL( MagickUnsharpMaskImage( magick_wand, radius, sigma, amount, threshold ) );
	}
	else {
		MW_CHECK_CONSTANT( ChannelType, channel );
		MW_BOOL_FUNC_RETVAL_BOOL( MagickUnsharpMaskImageChannel( magick_wand, (ChannelType) channel, radius, sigma, amount, threshold ) );
	}
}
/* }}} */

/* {{{ proto bool MagickWaveImage( MagickWand magick_wand, float amplitude, float wave_length )
*/
PHP_FUNCTION( magickwaveimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	double amplitude, wavelength;

	MW_GET_3_ARG( "rdd", &magick_wand_rsrc_zvl_p, &amplitude, &wavelength );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickWaveImage( magick_wand, amplitude, wavelength ) );
}
/* }}} */

/* {{{ proto bool MagickWhiteThresholdImage( MagickWand magick_wand, mixed threshold_pixel_wand )
*/
PHP_FUNCTION( magickwhitethresholdimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	PixelWand *threshold_pixel_wand;
	zval ***zvl_pp_args_arr;
	int arg_count, is_script_pixel_wand;

	MW_GET_ARGS_ARRAY_EX(	arg_count, (arg_count != 2),
							zvl_pp_args_arr,
							MagickWand, magick_wand,
							"a MagickWand resource and a threshold color PixelWand resource "  \
							"(or ImageMagick color string)" );

	MW_SETUP_PIXELWAND_FROM_ARG_ARRAY( zvl_pp_args_arr, 1, 2, threshold_pixel_wand, is_script_pixel_wand );

	MW_BOOL_FUNC_RETVAL_BOOL( MagickWhiteThresholdImage( magick_wand, threshold_pixel_wand ) );

	efree( zvl_pp_args_arr );

	if ( !is_script_pixel_wand ) {
		threshold_pixel_wand = DestroyPixelWand( threshold_pixel_wand );
	}
}
/* }}} */

/* {{{ proto bool MagickWriteImage( MagickWand magick_wand, string filename )
*/
PHP_FUNCTION( magickwriteimage )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *filename_from_script,
		 *filename,
		 *name, *ext,
		 *img_filename,
		 *wand_filename,
		 *tmp_filename,
		 *orig_img_format;
	int filename_from_script_len = 0, field_width;
	long img_idx;
	size_t wand_filename_len, tmp_filename_len;
	MagickBooleanType img_had_format;

	filename_from_script =
		filename =
		name =
		img_filename =
		orig_img_format =
		wand_filename =
		tmp_filename = (char *) NULL;

	MW_GET_3_ARG( "r|s", &magick_wand_rsrc_zvl_p, &filename_from_script, &filename_from_script_len );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	/* Saves the current active image index, as well as performs a shortcut check for any exceptions,
	   as they will occur if magick_wand contains no images
	*/
	img_idx = (long) MagickGetIteratorIndex( magick_wand );
	if ( MagickGetExceptionType(magick_wand) != UndefinedException  ) {
		RETURN_FALSE;
	}
	MagickClearException( magick_wand );

	MW_ENSURE_IMAGE_HAS_FORMAT( magick_wand, img_idx, img_had_format, orig_img_format, "MagickWriteImage" );

	if ( filename_from_script_len > 0 ) {
		/* Point filename string to filename_from_script; make them reference same string */
		filename = filename_from_script;
	}
	else {
		img_filename = (char *) MagickGetImageFilename( magick_wand );

		if ( img_filename == (char *) NULL || *img_filename == '\0' ) {

			wand_filename = (char *) MagickGetFilename( magick_wand );

			if ( wand_filename == (char *) NULL || *wand_filename == '\0' ) {
				/* No filename at all here, output fatal error, with reason */
				zend_error( MW_E_ERROR, "%s(): the filename argument was empty and, neither the filename of the "  \
										"current image (index %ld) nor that of the MagickWand resource argument was set; "  \
										"either supply this function with a filename argument, set the current active image's "  \
										"filename, or, set the MagickWand's filename, BEFORE calling this function",
										get_active_function_name(TSRMLS_C), img_idx );
				MW_FREE_MAGICK_MEM( char *, img_filename  );
				MW_FREE_MAGICK_MEM( char *, wand_filename );
				return;
			}

			/* The following sets up the minimum width of the numbers that will be inserted into the
			   image's new filename, based on the MagickWand's filename, if the image has no filename.
			   The pattern that the image's new filename will match, will be, if magick_wand's filename is
			   "testimage.jpg", and current image index is 5 / 150, and image has no filename, then the
			   image's new filename will be:
			   testimage_005.jpg */
			MW_SET_ZERO_FILLED_WIDTH( magick_wand, field_width );

			wand_filename_len = strlen( wand_filename );

			if ( MW_split_filename_on_period( &wand_filename, wand_filename_len,
											  &name, field_width, &ext,
											  &tmp_filename, &tmp_filename_len ) == MagickFalse )
			{
				MW_FREE_MAGICK_MEM( char *, img_filename  );
				MW_FREE_MAGICK_MEM( char *, wand_filename );
				return;
			}

			/* join name to i (made into field_width width), separated by an underscore, then append ext to form the tmp_filename
			*/
			snprintf( tmp_filename, tmp_filename_len, "%s_%.*ld%s", name, field_width, img_idx, ext );

			filename = tmp_filename;  /* Point filename string to tmp_filename; make them reference same string */
		}
		else {
			filename = img_filename;  /* Point filename string to img_filename; make them reference same string */
		}
	}

	if ( MW_write_image( magick_wand, filename, img_idx ) == MagickTrue ) {

		/* Check if current image had a format before function started */
		if ( img_had_format == MagickTrue ) {
			RETVAL_TRUE;
		}
		else {
			MW_DEBUG_NOTICE_EX( "Attempting to reset image #%d's image format", img_idx );

			/* If not, it must have been changed to the MagickWand's format, so set it back (probably to nothing) */
			if ( MagickSetImageFormat( magick_wand, orig_img_format ) == MagickTrue ) {

				MW_DEBUG_NOTICE_EX(	"SUCCESS: reset image #%d's image format", img_idx );

				RETVAL_TRUE;
			}
			else {
				MW_DEBUG_NOTICE_EX(	"FAILURE: could not reset image #%d's image format", img_idx );

				/* C API could not reset set the current image's format back to its original state: output error, with reason */
				MW_API_FUNC_FAIL_CHECK_WAND_ERROR_EX_1(	magick_wand, MagickWand,
														"unable to set the image at MagickWand index %ld back to "  \
														"its original image format",
														img_idx
				);
			}
		}
	}

	MW_FREE_MAGICK_MEM( char *, orig_img_format );

	MW_FREE_MAGICK_MEM( char *, img_filename  );
	MW_FREE_MAGICK_MEM( char *, wand_filename );
	MW_EFREE_MEM( char *, name          );
	MW_EFREE_MEM( char *, tmp_filename  );
}
/* }}} */

/* {{{ proto bool MagickWriteImageFile( MagickWand magick_wand, php_stream handle )
*/
PHP_FUNCTION( magickwriteimagefile )
{
	zend_error(	MW_E_ERROR, "%s(): This function is not yet implemented", get_active_function_name(TSRMLS_C) );
}
/* }}} */

/* {{{ proto bool MagickWriteImages( MagickWand magick_wand [, string filename [, bool join_images]] )
*/
PHP_FUNCTION( magickwriteimages )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	char *filename_from_script,
		 *wand_filename,
		 *wand_format,
		 *img_format,
		 *filename,
		 *name, *ext,
		 *img_filename,
		 *tmp_img_filename,
		 *new_img_filename,
		 *orig_img_format;
	int filename_from_script_len = 0;
	zend_bool join_images = FALSE;
	long initial_index, cur_img_idx;
	int field_width = 0; // silence gcc warning
	size_t filename_len = 0, tmp_img_filename_len;
	MagickBooleanType img_had_format;

	filename_from_script =
		wand_filename =
		wand_format =
		img_format =
		filename =
		name =
		img_filename =
		tmp_img_filename =
		new_img_filename = (char *) NULL;

	MW_GET_4_ARG( "r|sb",
				  &magick_wand_rsrc_zvl_p,
				  &filename_from_script, &filename_from_script_len,
				  &join_images );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	/* Saves the current active image index, plus performs shortcut check for any exceptions,
	   which will occur if magick_wand contains no images
	*/
	initial_index = (long) MagickGetIteratorIndex( magick_wand );

	if ( MagickGetExceptionType(magick_wand) != UndefinedException  ) {
		RETURN_FALSE;
	}
	MagickClearException( magick_wand );

	if ( filename_from_script != (char *) NULL ) {
		filename = filename_from_script;
		filename_len = (size_t) filename_from_script_len;
	}
	else {
		wand_filename = (char *) MagickGetFilename( magick_wand );

		if ( !( wand_filename == (char *) NULL || *wand_filename == '\0' ) ) {
			filename = wand_filename;
			filename_len = strlen( filename );
		}
	}

	if ( join_images == TRUE ) {
		if ( filename_len < 1 ) {
			/* No filename at all here, output fatal error, with reason */
			MW_SPIT_FATAL_ERR( "the filename sent to the function was empty, and the MagickWand resource does not "  \
							   "have a filename set; the MagickWand's images cannot be joined and written as 1 image" );
		}
		else {
			wand_format = (char *) MagickGetFormat( magick_wand );

			if ( wand_format == (char *) NULL || *wand_format == '\0' || *wand_format == '*' ) {
				MW_SPIT_FATAL_ERR(	"the MagickWand resource sent to this function did not have an image format set "  \
									"(via MagickSetFormat()); the MagickWand's image format must be set in order "  \
									"for MagickWriteImages() to continue with the join_images argument (the "  \
									"optional 3rd argument) as FALSE (its default)" );
			}
			else {
				MW_DEBUG_NOTICE_EX( "Attempting to write images to file \"%s\"", filename );

				if ( MW_write_images( magick_wand, filename ) == MagickTrue ) {
					MW_DEBUG_NOTICE_EX( "Wrote images to file \"%s\"", filename );
					RETVAL_TRUE;
				}
			}

			MW_FREE_MAGICK_MEM( char *, wand_format );
		}

		MW_FREE_MAGICK_MEM( char *, wand_filename );
		return;
	}
	else {
		/* Check if there is a valid filename "registered", and if so, prepare the parts of the
		   filenames to be substituted if any images don't have filenames:
		   name + _ + cur_img_idx (field_width fixed width) + ext (e.g.: testimage_08.jpg).
		   If all the images have filenames, this never matters; if any don't, this comes into effect.
		   If any images don't have filenames, and the filename_len is <= 0 (i.e. no "external"
		   filename, an error will occur, and the script will exit.
		*/
		if ( filename_len > 0 ) {
			/* The following sets up the minimum width of the numbers that will be inserted into the
			   image's new filename, based on the MagickWand's filename, if the image has no filename.
			   The pattern that the image's new filename will match, will be, if magick_wand's filename is
			   "testimage.jpg", and current image index is 5 / 150, and image has no filename, then the
			   image's new filename will be:
			   testimage_005.jpg
			*/
			MW_SET_ZERO_FILLED_WIDTH( magick_wand, field_width );

			MW_DEBUG_NOTICE_EX( "Setting up \"%s\" for incrementing...", filename );

			if ( MW_split_filename_on_period( &filename, filename_len,
											  &name, field_width, &ext,
											  &tmp_img_filename, &tmp_img_filename_len ) == MagickFalse )
			{
				MW_FREE_MAGICK_MEM( char *, wand_filename );
				return;
			}

			MW_DEBUG_NOTICE( "magickwriteimages(): MW_split_filename_on_period() function completed successfully" );
			MW_DEBUG_NOTICE_EX( "magickwriteimages(): name = \"%s\"", (name == (char *) NULL ? "NULL" : name) );
			MW_DEBUG_NOTICE_EX( "magickwriteimages(): ext = \"%s\"", ext );
			MW_DEBUG_NOTICE_EX( "magickwriteimages(): tmp_img_filename_len = %d", tmp_img_filename_len );
		}

		MW_DEBUG_NOTICE_EX( "The initial image index was %d", initial_index );

		MW_DEBUG_NOTICE( "Resetting MagickWand's iterator" );
		MagickResetIterator( magick_wand ); /* Start from the beginning... */

		MW_DEBUG_NOTICE( "Getting MagickWand's image format" );
		wand_format = (char *) MagickGetFormat( magick_wand );

		for ( cur_img_idx = 0; MagickNextImage( magick_wand ) == MagickTrue; cur_img_idx++ ) {

			MW_DEBUG_NOTICE_EX( "Checking image #%d for an image format", cur_img_idx );

			img_format = (char *) MagickGetImageFormat( magick_wand );
			if ( img_format == (char *) NULL || *img_format == '\0' || *img_format == '*' ) {

				MW_DEBUG_NOTICE_EX( "Image #%d has no image format", cur_img_idx );

				MW_FREE_MAGICK_MEM( char *, img_format );

				img_had_format = MagickFalse;

				if ( wand_format == (char *) NULL || *wand_format == '\0' || *wand_format == '*' ) {
					zend_error( MW_E_ERROR, "%s: neither the MagickWand resource sent to this function, nor its current "  \
											"active image (index %ld) had an image format set (via MagickSetFormat() or "  \
											"MagickSetImageFormat()); the function checks for the current active "  \
											"image's image format, and then for the MagickWand's image format -- "  \
											"one of them must be set in order for MagickWriteImages() to continue",
											get_active_function_name(TSRMLS_C), cur_img_idx );
					break;
				}
				else {
					MW_DEBUG_NOTICE_EX( "Attempting to set image #%d's image format to the MagickWand's image format",
										cur_img_idx );

					if ( MagickSetImageFormat( magick_wand, wand_format ) == MagickTrue ) {

						MW_DEBUG_NOTICE_EX(	"SUCCESS: set image #%d's image format to the MagickWand's image format",
											cur_img_idx );

						orig_img_format = (char *) NULL;
					}
					else {
						MW_DEBUG_NOTICE_EX(	"FAILURE: could not set image #%d's image format to the MagickWand's image format",
											cur_img_idx );

						/* C API cannot set the current image's format to the MagickWand's format: output error, with reason */
						MW_API_FUNC_FAIL_CHECK_WAND_ERROR_EX_2(	magick_wand, MagickWand,
																"unable to set the format of the image at index %ld "  \
																"to the MagickWand's set image format \"%s\"",
																cur_img_idx, wand_format
						);

						break;
					}
				}
			}
			else {
				MW_DEBUG_NOTICE_EX( "Image #%d had an image format", cur_img_idx );

				img_had_format = MagickTrue;

				orig_img_format = img_format;
			}

			MW_DEBUG_NOTICE_EX( "Setting up image #%d's filename", cur_img_idx );

			/* If there was a filename argument, or a MagickWand filename... */
			if ( filename_len > 0 ) {

				MW_DEBUG_NOTICE( "A filename argument or MagickWand filename exists, so:" );

				/* ... join name to cur_img_idx (made into field_width width), separated by an
				   underscore, then append ext to form the tmp_img_filename
				*/
				snprintf( tmp_img_filename, tmp_img_filename_len, "%s_%*.*ld%s", name, field_width, field_width, cur_img_idx, ext );

				MW_DEBUG_NOTICE_EX( "tmp_img_filename = \"%s\"", tmp_img_filename );

				/* Point new_img_filename string to tmp_img_filename; make them reference same string */
				new_img_filename = tmp_img_filename;
			}
			else { /* ... otherwise check each image for a filename */
				MW_DEBUG_NOTICE( "NO filename argument or MagickWand filename exists, so..." );

				img_filename = (char *) MagickGetImageFilename( magick_wand );

				if ( img_filename == (char *) NULL || *img_filename == '\0' ) {
					/* Bad img_filename; new_img_filename gets set to an invalid string */
					new_img_filename = (char *) NULL;

					MW_DEBUG_NOTICE( "there is NO image filename" );
				}
				else {
					/* Point new_img_filename string to img_filename; make them reference same string */
					new_img_filename = img_filename;
					MW_DEBUG_NOTICE_EX( "img_filename = \"%s\"", img_filename );
				}
			}

			MW_DEBUG_NOTICE_EX( "new_img_filename = \"%s\"", (new_img_filename == (char *) NULL ? "NULL" : new_img_filename) );

			/* Is the new_img_filename valid? */

			/* If the new_img_filename is INVALID... */
			if ( new_img_filename == (char *) NULL ) {
				/* No filename at all here, output error, with reason */
				zend_error( MW_E_ERROR, "%s(): the filename sent to this function was empty and, neither "  \
										"the image at MagickWand index %ld's filename nor the MagickWand "  \
										"resource's filename was set; either supply this function with a "  \
										"filename argument or, set the image's filename or the "  \
										"MagickWand's filename, BEFORE calling this function",
										get_active_function_name(TSRMLS_C), cur_img_idx );

				MW_FREE_MAGICK_MEM( char *, orig_img_format );
				MW_FREE_MAGICK_MEM( char *, img_filename );

				break;
			}
			else { /* the new_img_filename is valid */
				MW_DEBUG_NOTICE_EX( "new_img_filename = \"%s\"", new_img_filename );

				if ( MW_write_image( magick_wand, new_img_filename, cur_img_idx ) == MagickFalse ) {

					MW_FREE_MAGICK_MEM( char *, orig_img_format );
					MW_FREE_MAGICK_MEM( char *, img_filename );

					break;
				}
				else {
					MW_DEBUG_NOTICE_EX( "Wrote images to file \"%s\"", new_img_filename );

					/* Check if current image had a format before function started */
					if ( img_had_format == MagickFalse ) {

						MW_DEBUG_NOTICE_EX( "Attempting to reset image #%d's image format", cur_img_idx );

						/* If not, it must have been changed to the MagickWand's format, so set it back (probably to nothing) */
						if ( MagickSetImageFormat( magick_wand, orig_img_format ) == MagickFalse ) {

							MW_DEBUG_NOTICE_EX(	"FAILURE: could not reset image #%d's image format", cur_img_idx );

							/* C API could not reset set the current image's format back to its original state:
							   output error, with reason
							*/
							MW_API_FUNC_FAIL_CHECK_WAND_ERROR_EX_1(	magick_wand, MagickWand,
																	"unable to set the image at MagickWand index %ld back to "  \
																	"its original image format",
																	cur_img_idx
							);

							MW_FREE_MAGICK_MEM( char *, orig_img_format );
							MW_FREE_MAGICK_MEM( char *, img_filename );

							break;
						}
					}
				}
			}
			MW_FREE_MAGICK_MEM( char *, orig_img_format );
			MW_FREE_MAGICK_MEM( char *, img_filename );
		}

		MW_EFREE_MEM( char *, name );
		MW_EFREE_MEM( char *, tmp_img_filename  );

		MW_FREE_MAGICK_MEM( char *, wand_format );

		MW_FREE_MAGICK_MEM( char *, wand_filename );

		/* If all the above was successful, set the image index back to what it was */
		if ( MagickSetIteratorIndex( magick_wand, initial_index ) == MagickFalse ) {

			/* Just in case error occurs in MagickSetIteratorIndex() */
			MW_API_FUNC_FAIL_CHECK_WAND_ERROR_EX_1(	magick_wand, MagickWand,
													"cannot reset the MagickWand index to %ld",
													initial_index
			);
		}
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool MagickWriteImagesFile( MagickWand magick_wand, php_stream handle )
*/
PHP_FUNCTION( magickwriteimagesfile )
{
	zend_error(	MW_E_ERROR, "%s(): This function is not yet implemented", get_active_function_name(TSRMLS_C) );
}
/* }}} */



/*
****************************************************************************************************
*************************     PixelIterator Functions     *************************
****************************************************************************************************
*/

/* {{{ proto void ClearPixelIterator( PixelIterator pixel_iterator )
*/
PHP_FUNCTION( clearpixeliterator )
{
	MW_PRINT_DEBUG_INFO

	PixelIterator *pixel_iterator;
	zval *pixel_iterator_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &pixel_iterator_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( PixelIterator, pixel_iterator, &pixel_iterator_rsrc_zvl_p );

	ClearPixelIterator( pixel_iterator );
}
/* }}} */

/* {{{ proto void DestroyPixelIterator( PixelIterator pixel_iterator )
*/
PHP_FUNCTION( destroypixeliterator )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_RSRC_DESTROY_POINTER( PixelIterator );
}
/* }}} */

/* {{{ proto PixelIterator NewPixelIterator( MagickWand magick_wand )
*/
PHP_FUNCTION( newpixeliterator )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &magick_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( PixelIterator, NewPixelIterator( magick_wand ) );
}
/* }}} */

/* {{{ proto PixelIterator NewPixelRegionIterator( MagickWand magick_wand, int x, int y, int columns, int rows )
*/
PHP_FUNCTION( newpixelregioniterator )
{
	MW_PRINT_DEBUG_INFO

	MagickWand *magick_wand;
	zval *magick_wand_rsrc_zvl_p;
	long x, y;
	double columns, rows;

	MW_GET_5_ARG( "rlldd", &magick_wand_rsrc_zvl_p, &x, &y, &columns, &rows );

	MW_GET_POINTER_FROM_RSRC( MagickWand, magick_wand, &magick_wand_rsrc_zvl_p );

	MW_SET_RET_WAND_RSRC_FROM_FUNC( PixelIterator, NewPixelRegionIterator( magick_wand, x, y, (unsigned long) columns, (unsigned long) rows ) );
}
/* }}} */

/* {{{ proto array PixelGetIteratorException( PixelIterator pixel_iterator )
*/
PHP_FUNCTION( pixelgetiteratorexception )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_ARR( PixelIterator );
}
/* }}} */

/* {{{ proto string PixelGetIteratorExceptionString( PixelIterator pixel_iterator )
*/
PHP_FUNCTION( pixelgetiteratorexceptionstring )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_STR( PixelIterator );
}
/* }}} */

/* {{{ proto int PixelGetIteratorExceptionType( PixelIterator pixel_iterator )
*/
PHP_FUNCTION( pixelgetiteratorexceptiontype )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_TYPE( PixelIterator );
}
/* }}} */

/* {{{ proto array PixelGetNextIteratorRow( PixelIterator pixel_iterator )
*/
PHP_FUNCTION( pixelgetnextiteratorrow )
{
	MW_PRINT_DEBUG_INFO

/*  The following is only used in this function to ensure that the right kind of PixelWand array
	is returned to the script, i.e. an array of PixelIterator PixelWands that WILL not be auto
	cleaned up by PHP, but which will only b destroyed when the pixel_iterator is cleaned up by
	ImageMagick
*/
	MW_CHECK_ERR_RET_PixelIteratorPixelWand_ARR( PixelGetNextIteratorRow );
}
/* }}} */

/* {{{ proto array PixelGetPreviousIteratorRow( PixelIterator pixel_iterator )
*/
PHP_FUNCTION( pixelgetpreviousiteratorrow )
{
	MW_PRINT_DEBUG_INFO

/*  The following is only used in this function to ensure that the right kind of PixelWand array
	is returned to the script, i.e. an array of PixelIterator PixelWands that WILL not be auto
	cleaned up by PHP, but which will only b destroyed when the pixel_iterator is cleaned up by
	ImageMagick
*/
	MW_CHECK_ERR_RET_PixelIteratorPixelWand_ARR( PixelGetPreviousIteratorRow );
}
/* }}} */

/* {{{ proto void PixelResetIterator( PixelIterator pixel_iterator )
*/
PHP_FUNCTION( pixelresetiterator )
{
	MW_PRINT_DEBUG_INFO

	PixelIterator *pixel_iterator;
	zval *pixel_iterator_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &pixel_iterator_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( PixelIterator, pixel_iterator, &pixel_iterator_rsrc_zvl_p );

	PixelResetIterator( pixel_iterator );
}
/* }}} */

/* {{{ proto void PixelSetFirstIteratorRow( PixelIterator pixel_iterator )
*/
PHP_FUNCTION( pixelsetfirstiteratorrow )
{
	MW_PRINT_DEBUG_INFO

	PixelIterator *pixel_iterator;
	zval *pixel_iterator_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &pixel_iterator_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( PixelIterator, pixel_iterator, &pixel_iterator_rsrc_zvl_p );

	PixelSetFirstIteratorRow( pixel_iterator );
}
/* }}} */

/* {{{ proto bool PixelSetIteratorRow( PixelIterator pixel_iterator, int row )
*/
PHP_FUNCTION( pixelsetiteratorrow )
{
	MW_PRINT_DEBUG_INFO

	PixelIterator *pixel_iterator;
	zval *pixel_iterator_rsrc_zvl_p;
	long row;

	MW_GET_2_ARG( "rl", &pixel_iterator_rsrc_zvl_p, &row );

	MW_GET_POINTER_FROM_RSRC( PixelIterator, pixel_iterator, &pixel_iterator_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( PixelSetIteratorRow( pixel_iterator, row ) );
}
/* }}} */

/* {{{ proto void PixelSetLastIteratorRow( PixelIterator pixel_iterator )
*/
PHP_FUNCTION( pixelsetlastiteratorrow )
{
	MW_PRINT_DEBUG_INFO

	PixelIterator *pixel_iterator;
	zval *pixel_iterator_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &pixel_iterator_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( PixelIterator, pixel_iterator, &pixel_iterator_rsrc_zvl_p );

	PixelSetLastIteratorRow( pixel_iterator );
}
/* }}} */

/* {{{ proto bool PixelSyncIterator( PixelIterator pixel_iterator )
*/
PHP_FUNCTION( pixelsynciterator )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_FUNC_BOOL( PixelIterator, PixelSyncIterator );
}
/* }}} */


/*
****************************************************************************************************
*************************     PixelWand Functions     *************************
****************************************************************************************************
*/

/* {{{ proto void ClearPixelWand( PixelWand pixel_wand )
*/
PHP_FUNCTION( clearpixelwand )
{
	MW_PRINT_DEBUG_INFO

	PixelWand *pixel_wand;
	zval *pixel_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &pixel_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( PixelWand, pixel_wand, &pixel_wand_rsrc_zvl_p );

	ClearPixelWand( pixel_wand );
}
/* }}} */

/* {{{ proto PixelWand ClonePixelWand( PixelWand pixel_wand )
*/
PHP_FUNCTION( clonepixelwand )
{
	MW_PRINT_DEBUG_INFO

	PixelWand *pixel_wand, *clone_pixel_wand;
	zval *pixel_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &pixel_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( PixelWand, pixel_wand, &pixel_wand_rsrc_zvl_p );

	clone_pixel_wand = (PixelWand *) NewPixelWand();

	MW_SET_RET_WAND_RSRC( PixelWand, clone_pixel_wand );

	PixelSetRed(    clone_pixel_wand, PixelGetRed(    pixel_wand));
	PixelSetGreen(  clone_pixel_wand, PixelGetGreen(  pixel_wand));
	PixelSetBlue(   clone_pixel_wand, PixelGetBlue(   pixel_wand));
	PixelSetOpacity(clone_pixel_wand, PixelGetOpacity(pixel_wand));
}
/* }}} */

/* {{{ proto void DestroyPixelWand( PixelWand pixel_wand )
*/
PHP_FUNCTION( destroypixelwand )
{
	MW_PRINT_DEBUG_INFO

	PixelWand *pixel_wand;
	zval *pixel_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &pixel_wand_rsrc_zvl_p );

	/*  This specifically for the DestroyPixelWand*() functions -- can't have them trying to destroy
		PixelIterator PixelWands
	*/
	MW_DESTROY_ONLY_PixelWand( pixel_wand, &pixel_wand_rsrc_zvl_p );
}
/* }}} */

/* {{{ proto void DestroyPixelWandArray( array pixel_wand_array )
*/
PHP_FUNCTION( destroypixelwandarray )
{
	MW_PRINT_DEBUG_INFO

	PixelWand *pixel_wand;
	zval *zvl_arr, **zvl_pp_element;
	int num_wnds;
	HashPosition pos;

	MW_GET_1_ARG( "a", &zvl_arr );

	num_wnds = zend_hash_num_elements( Z_ARRVAL_P( zvl_arr ) );

	if ( num_wnds < 1 ) {
		MW_SPIT_FATAL_ERR( "array argument must contain at least 1 PixelWand resource" );
	}

	/* Makes sure no attempts to destroy PixelIterator PixelWands are made by explicitly
	   retrieving only "pure" (as far as PHP is concerned) PixelWands
	*/
	MW_ITERATE_OVER_PHP_ARRAY( pos, zvl_arr, zvl_pp_element ) {
		if ( MW_FETCH_RSRC( PixelWand, pixel_wand, zvl_pp_element ) == MagickFalse || IsPixelWand( pixel_wand ) == MagickFalse ) {
			zend_error( MW_E_ERROR, "%s(): function can only act on an array of PixelWand resources; "  \
									"(NOTE: PixelWands derived from PixelIterators are also invalid)",
									get_active_function_name(TSRMLS_C) );
			return;
		}
		zend_list_delete( Z_LVAL_PP( zvl_pp_element ) );
	}
/*
	PixelWand *pixel_wand;
	int arg_count = ZEND_NUM_ARGS(), i;
	zval ***zvl_pp_args_arr, **zvl_pp_element;
	HashPosition pos;

	if ( arg_count < 1 ) {
		zend_error( MW_E_ERROR, "%s(): error in function call: function requires at least 1 parameter -- "  \
								"at least 1 of either (or both) of the following: PixelWand resources, "  \
								"or arrays of PixelWand resources",
								get_active_function_name(TSRMLS_C) );
		return;
	}

	MW_ARR_ECALLOC( zval **, zvl_pp_args_arr, arg_count );

	if ( zend_get_parameters_array_ex( arg_count, zvl_pp_args_arr ) == FAILURE ) {
		zend_error( MW_E_ERROR, "%s(): unknown error in function call", get_active_function_name(TSRMLS_C) );
		efree( zvl_pp_args_arr );
		return;
	}

#define PRV_DESTROY_THE_PIXELWAND( pixel_wand_rsrc_zvl_pp )  \
{   if ( MW_FETCH_RSRC( PixelWand, pixel_wand, pixel_wand_rsrc_zvl_pp ) == MagickFalse || IsPixelWand( pixel_wand ) == MagickFalse ) {  \
		zend_error( MW_E_ERROR,  \
					"%s(): function can act only on PixelWand resources or arrays of PixelWand resources; "  \
					"argument #%d was neither. "  \
					"(NOTE: PixelWands derived from PixelIterators are also invalid)",  \
					get_active_function_name(TSRMLS_C), i );  \
		efree( zvl_pp_args_arr );  \
		return;  \
	}  \
	zend_list_delete( Z_LVAL_PP( zvl_pp_args_arr[i] ) );  \
}

	for ( i = 0; i < arg_count; i++ ) {
		switch ( Z_TYPE_PP( zvl_pp_args_arr[i] ) ) {
			case IS_RESOURCE : PRV_DESTROY_THE_PIXELWAND( zvl_pp_args_arr[i] );
							   break;

			case IS_ARRAY    : MW_ITERATE_OVER_PHP_ARRAY( pos, *(zvl_pp_args_arr[i]), zvl_pp_element ) {
									PRV_DESTROY_THE_PIXELWAND( zvl_pp_element );
							   }
							   break;

			default          : MW_SPIT_FATAL_ERR(
									"argument(s) must either be individual PixelWand resources or arrays of Pixelwand resources" );
							   efree( zvl_pp_args_arr );
							   return;
		}
	}
	efree( zvl_pp_args_arr );
*/
}
/* }}} */

/* {{{ proto bool IsPixelWandSimilar( PixelWand pixel_wand_1, PixelWand pixel_wand_2 [, float fuzz] )
*/
PHP_FUNCTION( ispixelwandsimilar )
{
	MW_PRINT_DEBUG_INFO

	PixelWand *pixel_wand_1, *pixel_wand_2;
	zval *pixel_wand_1_rsrc_zvl_p, *pixel_wand_2_rsrc_zvl_p;
	double fuzz = 0.0;

	MW_GET_3_ARG( "rr|d", &pixel_wand_1_rsrc_zvl_p, &pixel_wand_2_rsrc_zvl_p, fuzz );

	MW_GET_POINTER_FROM_RSRC( PixelWand, pixel_wand_1, &pixel_wand_1_rsrc_zvl_p);
	MW_GET_POINTER_FROM_RSRC( PixelWand, pixel_wand_2, &pixel_wand_2_rsrc_zvl_p );

	MW_BOOL_FUNC_RETVAL_BOOL( IsPixelWandSimilar( pixel_wand_1, pixel_wand_2, fuzz ) );
}
/* }}} */

/* {{{ proto PixelWand NewPixelWand( [string imagemagick_col_str] )
*/
PHP_FUNCTION( newpixelwand )
{
	MW_PRINT_DEBUG_INFO

	PixelWand *new_pixel_wand;
	char *col_str;
	int col_str_len = 0;

	MW_GET_2_ARG( "|s", &col_str, &col_str_len );

	new_pixel_wand = (PixelWand *) NewPixelWand();

	MW_SET_RET_WAND_RSRC( PixelWand, new_pixel_wand );

	if ( col_str_len > 0 && PixelSetColor( new_pixel_wand, col_str ) == MagickFalse ) {

		MW_API_FUNC_FAIL_CHECK_WAND_ERROR(	new_pixel_wand, PixelWand,
											"could not set PixelWand to desired fill color"
		);
	}
}
/* }}} */

/* {{{ proto array NewPixelWandArray( int num_wnds )
*/
PHP_FUNCTION( newpixelwandarray )
{
	MW_PRINT_DEBUG_INFO

	long num_wnds_req;
	unsigned long num_wnds;
	PixelWand **pixel_wand_arr = (PixelWand **) NULL;

	MW_GET_1_ARG( "l", &num_wnds_req );

	if ( num_wnds_req < 1 ) {
		MW_SPIT_FATAL_ERR( "user must request 1 or more PixelWand resources" );
		return;
	}

	num_wnds = (unsigned long) num_wnds_req;

	pixel_wand_arr = (PixelWand **) NewPixelWands( num_wnds );

	if ( pixel_wand_arr == (PixelWand **) NULL ) {
		RETURN_FALSE;
	}
	else {
		MW_RET_RESOURCE_ARR( PixelWand, pixel_wand_arr, num_wnds, PixelWand );
	}
}
/* }}} */

/* {{{ proto float PixelGetAlpha( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetalpha )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetAlpha );
}
/* }}} */

/* {{{ proto float PixelGetAlphaQuantum( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetalphaquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetAlphaQuantum );
}
/* }}} */

/* {{{ proto array PixelGetException( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetexception )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_ARR( PixelWand );
}
/* }}} */

/* {{{ proto string PixelGetExceptionString( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetexceptionstring )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_STR( PixelWand );
}
/* }}} */

/* {{{ proto int PixelGetExceptionType( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetexceptiontype )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_EXCEPTION_TYPE( PixelWand );
}
/* }}} */

/* {{{ proto float PixelGetBlack( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetblack )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetBlack );
}
/* }}} */

/* {{{ proto float PixelGetBlackQuantum( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetblackquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetBlackQuantum );
}
/* }}} */

/* {{{ proto float PixelGetBlue( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetblue )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetBlue );
}
/* }}} */

/* {{{ proto float PixelGetBlueQuantum( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetbluequantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetBlueQuantum );
}
/* }}} */

/* {{{ proto string PixelGetColorAsString( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetcolorasstring )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RETVAL_STRING( PixelWand, PixelGetColorAsString );
}
/* }}} */

/* {{{ proto float PixelGetColorCount( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetcolorcount )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetColorCount );
}
/* }}} */

/* {{{ proto float PixelGetCyan( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetcyan )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetCyan );
}
/* }}} */

/* {{{ proto float PixelGetCyanQuantum( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetcyanquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetCyanQuantum );
}
/* }}} */

/* {{{ proto float PixelGetGreen( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetgreen )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetGreen );
}
/* }}} */

/* {{{ proto float PixelGetGreenQuantum( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetgreenquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetGreenQuantum );
}
/* }}} */

/* {{{ proto float PixelGetIndex( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetindex )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetIndex );
}
/* }}} */

/* {{{ proto float PixelGetMagenta( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetmagenta )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetMagenta );
}
/* }}} */

/* {{{ proto float PixelGetMagentaQuantum( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetmagentaquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetMagentaQuantum );
}
/* }}} */

/* {{{ proto float PixelGetOpacity( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetopacity )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetOpacity );
}
/* }}} */

/* {{{ proto float PixelGetOpacityQuantum( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetopacityquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetOpacityQuantum );
}
/* }}} */

/* {{{ proto array PixelGetQuantumColor( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetquantumcolor )
{
	MW_PRINT_DEBUG_INFO

	PixelWand *pixel_wand;
	PixelPacket pixel_packet;
	zval *pixel_wand_rsrc_zvl_p;

	MW_GET_1_ARG( "r", &pixel_wand_rsrc_zvl_p );

	MW_GET_POINTER_FROM_RSRC( PixelWand, pixel_wand, &pixel_wand_rsrc_zvl_p );

	PixelGetQuantumColor( pixel_wand, &pixel_packet );

	array_init( return_value );

	if (   add_assoc_double( return_value, "r", (double) pixel_packet.red     ) == FAILURE
		|| add_assoc_double( return_value, "g", (double) pixel_packet.green   ) == FAILURE
		|| add_assoc_double( return_value, "b", (double) pixel_packet.blue    ) == FAILURE
		|| add_assoc_double( return_value, "o", (double) pixel_packet.opacity ) == FAILURE )
	{
		MW_SPIT_FATAL_ERR( "error adding a value to the return array" );
	}
}
/* }}} */

/* {{{ proto float PixelGetRed( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetred )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetRed );
}
/* }}} */

/* {{{ proto float PixelGetRedQuantum( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetredquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetRedQuantum );
}
/* }}} */

/* {{{ proto float PixelGetYellow( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetyellow )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetYellow );
}
/* }}} */

/* {{{ proto float PixelGetYellowQuantum( PixelWand pixel_wand )
*/
PHP_FUNCTION( pixelgetyellowquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_RET_DOUBLE( PixelWand, PixelGetYellowQuantum );
}
/* }}} */

/* {{{ proto void PixelSetAlpha( PixelWand pixel_wand, float alpha )
*/
PHP_FUNCTION( pixelsetalpha )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( PixelWand, PixelSetAlpha );
}
/* }}} */

/* {{{ proto void PixelSetAlphaQuantum( PixelWand pixel_wand, float alpha )
*/
PHP_FUNCTION( pixelsetalphaquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_PIXELWAND_SET_QUANTUM_COLOR( PixelSetAlphaQuantum );
}
/* }}} */

/* {{{ proto void PixelSetBlack( PixelWand pixel_wand, float black )
*/
PHP_FUNCTION( pixelsetblack )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( PixelWand, PixelSetBlack );
}
/* }}} */

/* {{{ proto void PixelSetBlackQuantum( PixelWand pixel_wand, float black )
*/
PHP_FUNCTION( pixelsetblackquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_PIXELWAND_SET_QUANTUM_COLOR( PixelSetBlackQuantum );
}
/* }}} */

/* {{{ proto void PixelSetBlue( PixelWand pixel_wand, float blue )
*/
PHP_FUNCTION( pixelsetblue )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( PixelWand, PixelSetBlue );
}
/* }}} */

/* {{{ proto void PixelSetBlueQuantum( PixelWand pixel_wand, float blue )
*/
PHP_FUNCTION( pixelsetbluequantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_PIXELWAND_SET_QUANTUM_COLOR( PixelSetBlueQuantum );
}
/* }}} */

/* {{{ proto bool PixelSetColor( PixelWand pixel_wand, string imagemagick_col_str )
*/
PHP_FUNCTION( pixelsetcolor )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_AND_STRING_RETVAL_FUNC_BOOL( PixelWand, PixelSetColor );
}
/* }}} */

/* {{{ proto void PixelSetColorCount( PixelWand pixel_wand, int count )
*/
PHP_FUNCTION( pixelsetcolorcount )
{
	MW_PRINT_DEBUG_INFO

	PixelWand *pixel_wand;
	zval *pixel_wand_rsrc_zvl_p;
	long count;

	MW_GET_2_ARG( "rl", &pixel_wand_rsrc_zvl_p, &count );

	MW_GET_POINTER_FROM_RSRC( PixelWand, pixel_wand, &pixel_wand_rsrc_zvl_p );

	PixelSetColorCount( pixel_wand, (unsigned long) count );
}
/* }}} */

/* {{{ proto void PixelSetCyan( PixelWand pixel_wand, float cyan )
*/
PHP_FUNCTION( pixelsetcyan )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( PixelWand, PixelSetCyan );
}
/* }}} */

/* {{{ proto void PixelSetCyanQuantum( PixelWand pixel_wand, float cyan )
*/
PHP_FUNCTION( pixelsetcyanquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_PIXELWAND_SET_QUANTUM_COLOR( PixelSetCyanQuantum );
}
/* }}} */

/* {{{ proto void PixelSetGreen( PixelWand pixel_wand, float green )
*/
PHP_FUNCTION( pixelsetgreen )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( PixelWand, PixelSetGreen );
}
/* }}} */

/* {{{ proto void PixelSetGreenQuantum( PixelWand pixel_wand, float green )
*/
PHP_FUNCTION( pixelsetgreenquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_PIXELWAND_SET_QUANTUM_COLOR( PixelSetGreenQuantum );
}
/* }}} */

/* {{{ proto void PixelSetIndex( PixelWand pixel_wand, float index )
*/
PHP_FUNCTION( pixelsetindex )
{
	MW_PRINT_DEBUG_INFO

	PixelWand *pixel_wand;
	zval *pixel_wand_rsrc_zvl_p;
	double index;

	MW_GET_2_ARG( "rd", &pixel_wand_rsrc_zvl_p, &index );

	if ( index < 0.0 || index > MW_QuantumRange ) {
		zend_error( MW_E_ERROR, "%s(): the value of the colormap index argument (%0.0f) was invalid. "  \
								"The colormap index value must match \"0 <= index <= %0.0f\"",
								get_active_function_name( TSRMLS_C ), index, MW_QuantumRange );
		return;
	}

	MW_GET_POINTER_FROM_RSRC( PixelWand, pixel_wand, &pixel_wand_rsrc_zvl_p );

	PixelSetIndex( pixel_wand, (IndexPacket) index );
}
/* }}} */

/* {{{ proto void PixelSetMagenta( PixelWand pixel_wand, float magenta )
*/
PHP_FUNCTION( pixelsetmagenta )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( PixelWand, PixelSetMagenta );
}
/* }}} */

/* {{{ proto void PixelSetMagentaQuantum( PixelWand pixel_wand, float magenta )
*/
PHP_FUNCTION( pixelsetmagentaquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_PIXELWAND_SET_QUANTUM_COLOR( PixelSetMagentaQuantum );
}
/* }}} */

/* {{{ proto void PixelSetOpacity( PixelWand pixel_wand, float opacity )
*/
PHP_FUNCTION( pixelsetopacity )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( PixelWand, PixelSetOpacity );
}
/* }}} */

/* {{{ proto void PixelSetOpacityQuantum( PixelWand pixel_wand, float opacity )
*/
PHP_FUNCTION( pixelsetopacityquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_PIXELWAND_SET_QUANTUM_COLOR( PixelSetOpacityQuantum );
}
/* }}} */

/* {{{ proto void PixelSetQuantumColor( PixelWand pixel_wand, float red, float green, float blue [, float opacity] )
*/
PHP_FUNCTION( pixelsetquantumcolor )
{
	MW_PRINT_DEBUG_INFO

	PixelWand *pixel_wand;
	PixelPacket pixel_packet;
	zval *pixel_wand_rsrc_zvl_p;
	double red, green, blue, opacity = 0.0;

	MW_GET_5_ARG( "rddd|d", &pixel_wand_rsrc_zvl_p, &red, &green, &blue, &opacity );

	if (   red     < 0.0 || red     > MW_QuantumRange
		|| green   < 0.0 || green   > MW_QuantumRange
		|| blue    < 0.0 || blue    > MW_QuantumRange
		|| opacity < 0.0 || opacity > MW_QuantumRange )
	{
		zend_error( MW_E_ERROR, "%s(): the value of one or more of the Quantum color arguments was invalid. "  \
								"Quantum color values must match \"0 <= color_val <= %0.0f\"",
								get_active_function_name( TSRMLS_C ), MW_QuantumRange );
		return;
	}

	MW_GET_POINTER_FROM_RSRC( PixelWand, pixel_wand, &pixel_wand_rsrc_zvl_p );

	pixel_packet.red     = (Quantum) red;
	pixel_packet.green   = (Quantum) green;
	pixel_packet.blue    = (Quantum) blue;
	pixel_packet.opacity = (Quantum) opacity;

	PixelSetQuantumColor( pixel_wand, &pixel_packet );
}
/* }}} */

/* {{{ proto void PixelSetRed( PixelWand pixel_wand, float red )
*/
PHP_FUNCTION( pixelsetred )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( PixelWand, PixelSetRed );
}
/* }}} */

/* {{{ proto void PixelSetRedQuantum( PixelWand pixel_wand, float red )
*/
PHP_FUNCTION( pixelsetredquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_PIXELWAND_SET_QUANTUM_COLOR( PixelSetRedQuantum );
}
/* }}} */

/* {{{ proto void PixelSetYellow( PixelWand pixel_wand, float yellow )
*/
PHP_FUNCTION( pixelsetyellow )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_WAND_SET_NORMALIZED_COLOR( PixelWand, PixelSetYellow );
}
/* }}} */

/* {{{ proto void PixelSetYellowQuantum( PixelWand pixel_wand, float yellow )
*/
PHP_FUNCTION( pixelsetyellowquantum )
{
	MW_PRINT_DEBUG_INFO

	MW_GET_PIXELWAND_SET_QUANTUM_COLOR( PixelSetYellowQuantum );
}
/* }}} */

