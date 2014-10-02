PHP_ARG_WITH(magickwand, whether to enable the magickwand extension,
[ --with-magickwand[=DIR]	Enables the magickwand extension. DIR is the prefix to Imagemagick installation directory.], no)

if test $PHP_MAGICKWAND != "no"; then

    if test -r $PHP_MAGICKWAND/bin/MagickWand-config; then
        WAND_CONFIG_PATH=$PHP_MAGICKWAND/bin
    else
        AC_MSG_CHECKING(MagickWand-config in default path)

        for i in /usr/local /usr;
        do
            test -r $i/bin/MagickWand-config && WAND_CONFIG_PATH=$i/bin && break
        done

        if test -z "$WAND_CONFIG_PATH"; then
            for i in $PHP_MAGICKWAND /usr/local /usr;
            do
                test -r $i/MagickWand-config && WAND_CONFIG_PATH=$i && break
            done
        fi
        if test -z "$WAND_CONFIG_PATH"; then
            AC_MSG_ERROR(Cannot locate configuration program MagickWand-config)
        else
            AC_MSG_RESULT(found in $WAND_CONFIG_PATH)
        fi
    fi

		IMAGEMAGICK_VERSION_ORIG=`$WAND_CONFIG_PATH/MagickWand-config --version`
		IMAGEMAGICK_VERSION_MASK=`echo ${IMAGEMAGICK_VERSION_ORIG} | awk 'BEGIN { FS = "."; } { printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`


		AC_MSG_CHECKING(if ImageMagick version is at least 6.8.2)
		if test "$IMAGEMAGICK_VERSION_MASK" -ge 6008002; then
				AC_MSG_RESULT(found version $IMAGEMAGICK_VERSION_ORIG)
		else
				AC_MSG_ERROR(no. You need at least ImageMagick version 6.8.2 to use MagickWand for PHP.)
		fi

		AC_MSG_CHECKING(if PHP version is at least 4.1.3)

		tmp_version=$PHP_VERSION
		if test -z "$tmp_version"; then
  		if test -z "$PHP_CONFIG"; then
    			AC_MSG_ERROR([php-config not found])
  		fi
   			MAGICKWAND_PHP_VERSION_ORIG=`$PHP_CONFIG --version`;
		else
			MAGICKWAND_PHP_VERSION_ORIG=$tmp_version
		fi

		if test -z $MAGICKWAND_PHP_VERSION_ORIG; then
			AC_MSG_ERROR([failed to detect PHP version, please report])
		fi

		MAGICKWAND_PHP_VERSION_MASK=`echo ${MAGICKWAND_PHP_VERSION_ORIG} | awk 'BEGIN { FS = "."; } { printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`

		if test $MAGICKWAND_PHP_VERSION_MASK -ge 4001003; then
				AC_MSG_RESULT(found version $MAGICKWAND_PHP_VERSION_ORIG)
		else
				AC_MSG_ERROR(no. You need at least PHP version 4.1.3 to use MagickWand.)
		fi

		AC_DEFINE(HAVE_MAGICKWAND,1,[ ])

    export ORIG_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"
    export PKG_CONFIG_PATH="`$WAND_CONFIG_PATH/MagickWand-config --prefix`/lib/pkgconfig/"

		PHP_ADD_INCLUDE($WAND_DIR/include/ImageMagick)
		AC_MSG_CHECKING(MagickWand-config --cppflags)
		WAND_CPPFLAGS="`$WAND_CONFIG_PATH/MagickWand-config --cppflags`"
		AC_MSG_RESULT($WAND_CPPFLAGS)
		PHP_EVAL_INCLINE($WAND_CPPFLAGS)

		AC_MSG_CHECKING(MagickWand-config --libs)
		WAND_LIBS="`$WAND_CONFIG_PATH/MagickWand-config --libs`"
		AC_MSG_RESULT($WAND_LIBS)
		PHP_EVAL_LIBLINE($WAND_LIBS, MAGICKWAND_SHARED_LIBADD)
    export PKG_CONFIG_PATH="$ORIG_PKG_CONFIG_PATH"

		PHP_SUBST(MAGICKWAND_SHARED_LIBADD)
		PHP_NEW_EXTENSION(magickwand, magickwand.c, $ext_shared,,$WAND_CPPFLAGS)

fi
