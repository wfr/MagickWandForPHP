/* MagickWand for PHP module and user visible functions declaration file

   Author: Ouinnel Watson
   Homepage: 
   Release Date: 2006-12-30
*/

#ifndef PHP_MAGICKWAND_H
#define PHP_MAGICKWAND_H

extern zend_module_entry magickwand_module_entry;
#define phpext_magickwand_ptr &magickwand_module_entry

#ifdef PHP_WIN32
#define PHP_MAGICKWAND_API __declspec(dllexport)
#else
#define PHP_MAGICKWAND_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#define MAGICKWAND_VERSION "1.0.8"

/* ************************************************************************************************************** */

	ZEND_MINIT_FUNCTION( magickwand );
	ZEND_MSHUTDOWN_FUNCTION( magickwand );
/*
	ZEND_RINIT_FUNCTION( magickwand );
	ZEND_RSHUTDOWN_FUNCTION( magickwand );
*/
	ZEND_MINFO_FUNCTION( magickwand );

/*  Custom generic Wand exception functions
	-- can be used on any of the 4 MagickWand Wand/Iterator resource types
*/
	PHP_FUNCTION( wandgetexception );
	PHP_FUNCTION( wandgetexceptionstring );
	PHP_FUNCTION( wandgetexceptiontype );
	PHP_FUNCTION( wandhasexception );

	PHP_FUNCTION( isdrawingwand );
	PHP_FUNCTION( ismagickwand );
	PHP_FUNCTION( ispixeliterator );
	PHP_FUNCTION( ispixelwand );

	/* DrawingWand functions */

	PHP_FUNCTION( cleardrawingwand );
	PHP_FUNCTION( clonedrawingwand );
	PHP_FUNCTION( destroydrawingwand );
	PHP_FUNCTION( drawaffine );
	PHP_FUNCTION( drawannotation );
	PHP_FUNCTION( drawarc );

	PHP_FUNCTION( drawbezier );

	PHP_FUNCTION( drawcircle );
	PHP_FUNCTION( drawcolor );
	PHP_FUNCTION( drawcomment );
	PHP_FUNCTION( drawcomposite );
	PHP_FUNCTION( drawellipse );
	PHP_FUNCTION( drawgetclippath );
	PHP_FUNCTION( drawgetcliprule );
	PHP_FUNCTION( drawgetclipunits );

	PHP_FUNCTION( drawgetexception );
	PHP_FUNCTION( drawgetexceptionstring );
	PHP_FUNCTION( drawgetexceptiontype );

	PHP_FUNCTION( drawgetfillalpha );
	PHP_FUNCTION( drawgetfillcolor );
	PHP_FUNCTION( drawgetfillopacity );
	PHP_FUNCTION( drawgetfillrule );
	PHP_FUNCTION( drawgetfont );
	PHP_FUNCTION( drawgetfontfamily );
	PHP_FUNCTION( drawgetfontsize );
	PHP_FUNCTION( drawgetfontstretch );
	PHP_FUNCTION( drawgetfontstyle );
	PHP_FUNCTION( drawgetfontweight );
	PHP_FUNCTION( drawgetgravity );
	PHP_FUNCTION( drawgetstrokealpha );
	PHP_FUNCTION( drawgetstrokeantialias );
	PHP_FUNCTION( drawgetstrokecolor );
	PHP_FUNCTION( drawgetstrokedasharray );
	PHP_FUNCTION( drawgetstrokedashoffset );
	PHP_FUNCTION( drawgetstrokelinecap );
	PHP_FUNCTION( drawgetstrokelinejoin );
	PHP_FUNCTION( drawgetstrokemiterlimit );
	PHP_FUNCTION( drawgetstrokeopacity );
	PHP_FUNCTION( drawgetstrokewidth );
	PHP_FUNCTION( drawgettextalignment );
	PHP_FUNCTION( drawgettextantialias );
	PHP_FUNCTION( drawgettextdecoration );
	PHP_FUNCTION( drawgettextencoding );

	PHP_FUNCTION( drawgetvectorgraphics );

	PHP_FUNCTION( drawgettextundercolor );
	PHP_FUNCTION( drawline );
	PHP_FUNCTION( drawmatte );
	PHP_FUNCTION( drawpathclose );
	PHP_FUNCTION( drawpathcurvetoabsolute );
	PHP_FUNCTION( drawpathcurvetorelative );
	PHP_FUNCTION( drawpathcurvetoquadraticbezierabsolute );
	PHP_FUNCTION( drawpathcurvetoquadraticbezierrelative );
	PHP_FUNCTION( drawpathcurvetoquadraticbeziersmoothabsolute );
	PHP_FUNCTION( drawpathcurvetoquadraticbeziersmoothrelative );
	PHP_FUNCTION( drawpathcurvetosmoothabsolute );
	PHP_FUNCTION( drawpathcurvetosmoothrelative );
	PHP_FUNCTION( drawpathellipticarcabsolute );
	PHP_FUNCTION( drawpathellipticarcrelative );
	PHP_FUNCTION( drawpathfinish );
	PHP_FUNCTION( drawpathlinetoabsolute );
	PHP_FUNCTION( drawpathlinetorelative );
	PHP_FUNCTION( drawpathlinetohorizontalabsolute );
	PHP_FUNCTION( drawpathlinetohorizontalrelative );
	PHP_FUNCTION( drawpathlinetoverticalabsolute );
	PHP_FUNCTION( drawpathlinetoverticalrelative );
	PHP_FUNCTION( drawpathmovetoabsolute );
	PHP_FUNCTION( drawpathmovetorelative );
	PHP_FUNCTION( drawpathstart );
	PHP_FUNCTION( drawpoint );

	PHP_FUNCTION( drawpolygon );

	PHP_FUNCTION( drawpolyline );

	PHP_FUNCTION( drawpopclippath );
	PHP_FUNCTION( drawpopdefs );
	PHP_FUNCTION( drawpoppattern );
	PHP_FUNCTION( drawpushclippath );
	PHP_FUNCTION( drawpushdefs );
	PHP_FUNCTION( drawpushpattern );
	PHP_FUNCTION( drawrectangle );
	PHP_FUNCTION( drawrender );
	PHP_FUNCTION( drawrotate );
	PHP_FUNCTION( drawroundrectangle );
	PHP_FUNCTION( drawscale );
	PHP_FUNCTION( drawsetclippath );
	PHP_FUNCTION( drawsetcliprule );
	PHP_FUNCTION( drawsetclipunits );
	PHP_FUNCTION( drawsetfillalpha );
	PHP_FUNCTION( drawsetfillcolor );
	PHP_FUNCTION( drawsetfillopacity );
	PHP_FUNCTION( drawsetfillpatternurl );
	PHP_FUNCTION( drawsetfillrule );
	PHP_FUNCTION( drawsetfont );
	PHP_FUNCTION( drawsetfontfamily );
	PHP_FUNCTION( drawsetfontsize );
	PHP_FUNCTION( drawsetfontstretch );
	PHP_FUNCTION( drawsetfontstyle );
	PHP_FUNCTION( drawsetfontweight );
	PHP_FUNCTION( drawsetgravity );
	PHP_FUNCTION( drawsetstrokealpha );
	PHP_FUNCTION( drawsetstrokeopacity );
	PHP_FUNCTION( drawsetstrokeantialias );
	PHP_FUNCTION( drawsetstrokecolor );

	PHP_FUNCTION( drawsetstrokedasharray );

	PHP_FUNCTION( drawsetstrokedashoffset );
	PHP_FUNCTION( drawsetstrokelinecap );
	PHP_FUNCTION( drawsetstrokelinejoin );
	PHP_FUNCTION( drawsetstrokemiterlimit );
	PHP_FUNCTION( drawsetstrokepatternurl );
	PHP_FUNCTION( drawsetstrokewidth );
	PHP_FUNCTION( drawsettextalignment );
	PHP_FUNCTION( drawsettextantialias );
	PHP_FUNCTION( drawsettextdecoration );
	PHP_FUNCTION( drawsettextencoding );
	PHP_FUNCTION( drawsettextundercolor );
	PHP_FUNCTION( drawsetvectorgraphics );
	PHP_FUNCTION( drawskewx );
	PHP_FUNCTION( drawskewy );
	PHP_FUNCTION( drawtranslate );
	PHP_FUNCTION( drawsetviewbox );
	PHP_FUNCTION( newdrawingwand );
	PHP_FUNCTION( popdrawingwand );
	PHP_FUNCTION( pushdrawingwand );

	/* MagickWand functions */

	PHP_FUNCTION( clearmagickwand );
	PHP_FUNCTION( clonemagickwand );
	PHP_FUNCTION( destroymagickwand );
	PHP_FUNCTION( magickadaptivethresholdimage );

/* OW modification from C API convention:
      PHP's magickaddimage adds only the current active image from one wand to another,
	  while PHP's magickaddimages replicates the functionality of the C API's
	  MagickAddImage(), adding all the images of one wnad to another.

	  Minor semantics, but original funtionality proved annoying in actual usage.
*/
	PHP_FUNCTION( magickaddimage );
	PHP_FUNCTION( magickaddimages );

	PHP_FUNCTION( magickaddnoiseimage );
	PHP_FUNCTION( magickaffinetransformimage );
	PHP_FUNCTION( magickannotateimage );
	PHP_FUNCTION( magickappendimages );
	PHP_FUNCTION( magickaverageimages );
	PHP_FUNCTION( magickblackthresholdimage );

	PHP_FUNCTION( magickblurimage );
/*	PHP_FUNCTION( magickblurimagechannel );  */

	PHP_FUNCTION( magickborderimage );
	PHP_FUNCTION( magickcharcoalimage );
	PHP_FUNCTION( magickchopimage );
	PHP_FUNCTION( magickclipimage );
	PHP_FUNCTION( magickclippathimage );
	PHP_FUNCTION( magickcoalesceimages );
	PHP_FUNCTION( magickcolorfloodfillimage );
	PHP_FUNCTION( magickcolorizeimage );
	PHP_FUNCTION( magickcombineimages );
	PHP_FUNCTION( magickcommentimage );

	PHP_FUNCTION( magickcompareimages );
/*	PHP_FUNCTION( magickcompareimagechannels ); */

	PHP_FUNCTION( magickcompositeimage );
	PHP_FUNCTION( magickconstituteimage );
	PHP_FUNCTION( magickcontrastimage );

	PHP_FUNCTION( magickconvolveimage );
/*	PHP_FUNCTION( magickconvolveimagechannel );  */

	PHP_FUNCTION( magickcropimage );
	PHP_FUNCTION( magickcyclecolormapimage );
	PHP_FUNCTION( magickdeconstructimages );

	PHP_FUNCTION( magickdescribeimage );
	PHP_FUNCTION( magickidentifyimage );

	PHP_FUNCTION( magickdespeckleimage );

	PHP_FUNCTION( magickdisplayimage );
	PHP_FUNCTION( magickdisplayimages );

	PHP_FUNCTION( magickdrawimage );

	PHP_FUNCTION( magickechoimageblob );
	PHP_FUNCTION( magickechoimagesblob );

	PHP_FUNCTION( magickedgeimage );
	PHP_FUNCTION( magickembossimage );
	PHP_FUNCTION( magickenhanceimage );
	PHP_FUNCTION( magickequalizeimage );

	PHP_FUNCTION( magickevaluateimage );
/*	PHP_FUNCTION( magickevaluateimagechannel );  */

	PHP_FUNCTION( magickflattenimages );
	PHP_FUNCTION( magickflipimage );
	PHP_FUNCTION( magickflopimage );
	PHP_FUNCTION( magickframeimage );

	PHP_FUNCTION( magickfximage );
/*	PHP_FUNCTION( magickfximagechannel ); */

	PHP_FUNCTION( magickgammaimage );
/*	PHP_FUNCTION( magickgammaimagechannel ); */

	PHP_FUNCTION( magickgaussianblurimage );
/*	PHP_FUNCTION( magickgaussianblurimagechannel ); */

	PHP_FUNCTION( magickgetcompression );
	PHP_FUNCTION( magickgetcompressionquality );

	PHP_FUNCTION( magickgetcopyright );

	PHP_FUNCTION( magickgetexception );
	PHP_FUNCTION( magickgetexceptionstring );
	PHP_FUNCTION( magickgetexceptiontype );

	PHP_FUNCTION( magickgetfilename );
	PHP_FUNCTION( magickgetformat );
	PHP_FUNCTION( magickgethomeurl );
	PHP_FUNCTION( magickgetimage );
	PHP_FUNCTION( magickgetimagebackgroundcolor );
	PHP_FUNCTION( magickgetimageblob );
	PHP_FUNCTION( magickgetimagesblob );
	PHP_FUNCTION( magickgetimageblueprimary );
	PHP_FUNCTION( magickgetimagebordercolor );

	PHP_FUNCTION( magickgetimagechannelmean );

//	PHP_FUNCTION( magickgetimagechannelstatistics );

	PHP_FUNCTION( magickgetimageclipmask );
	PHP_FUNCTION( magickgetimagecolormapcolor );
	PHP_FUNCTION( magickgetimagecolors );
	PHP_FUNCTION( magickgetimagecolorspace );
	PHP_FUNCTION( magickgetimagecompose );
	PHP_FUNCTION( magickgetimagecompression );
	PHP_FUNCTION( magickgetimagecompressionquality );
	PHP_FUNCTION( magickgetimagedelay );

	PHP_FUNCTION( magickgetimagedepth );
/*	PHP_FUNCTION( magickgetimagechanneldepth ); */

	PHP_FUNCTION( magickgetimagedistortion );
/*	PHP_FUNCTION( magickgetimagechanneldistortion ); */

	PHP_FUNCTION( magickgetimagedispose );
	PHP_FUNCTION( magickgetimageendian );
	PHP_FUNCTION( magickgetimageattribute );

	PHP_FUNCTION( magickgetimageextrema );
/*	PHP_FUNCTION( magickgetimagechannelextrema ); */

	PHP_FUNCTION( magickgetimagefilename );
	PHP_FUNCTION( magickgetimageformat );
	PHP_FUNCTION( magickgetimagegamma );
	PHP_FUNCTION( magickgetimagegreenprimary );
	PHP_FUNCTION( magickgetimageheight );
	PHP_FUNCTION( magickgetimagehistogram );
	PHP_FUNCTION( magickgetimageindex );
	PHP_FUNCTION( magickgetimageinterlacescheme );
	PHP_FUNCTION( magickgetimageiterations );
	PHP_FUNCTION( magickgetimagemattecolor );

	PHP_FUNCTION( magickgetimagemimetype );
	PHP_FUNCTION( magickgetmimetype );

	PHP_FUNCTION( magickgetimagepage );

	PHP_FUNCTION( magickgetimagepixelcolor );

	PHP_FUNCTION( magickgetimagepixels );
	PHP_FUNCTION( magickgetimageprofile );
	PHP_FUNCTION( magickgetimageproperty );
	PHP_FUNCTION( magickgetimageredprimary );

	PHP_FUNCTION( magickgetimageregion );

	PHP_FUNCTION( magickgetimagerenderingintent );
	PHP_FUNCTION( magickgetimageresolution );
	PHP_FUNCTION( magickgetimagescene );
	PHP_FUNCTION( magickgetimagesignature );
	PHP_FUNCTION( magickgetimagesize );

	PHP_FUNCTION( magickgetimagetickspersecond );

	PHP_FUNCTION( magickgetimagetotalinkdensity );

	PHP_FUNCTION( magickgetimagetype );
	PHP_FUNCTION( magickgetimageunits );
	PHP_FUNCTION( magickgetimagevirtualpixelmethod );
	PHP_FUNCTION( magickgetimagewhitepoint );
	PHP_FUNCTION( magickgetimagewidth );
	PHP_FUNCTION( magickgetinterlacescheme );
	PHP_FUNCTION( magickgetnumberimages );

	PHP_FUNCTION( magickgetoption );

	PHP_FUNCTION( magickgetpackagename );

	PHP_FUNCTION( magickgetpage );

	PHP_FUNCTION( magickgetquantumdepth );

	PHP_FUNCTION( magickgetquantumrange );

	PHP_FUNCTION( magickgetreleasedate );

	PHP_FUNCTION( magickgetresource );

	PHP_FUNCTION( magickgetresourcelimit );
	PHP_FUNCTION( magickgetsamplingfactors );

/*	PHP_FUNCTION( magickgetsize ); */
	PHP_FUNCTION( magickgetwandsize );

	PHP_FUNCTION( magickgetversion );

/* OW added for convenience in getting the version as a number or string */
	PHP_FUNCTION( magickgetversionnumber );
	PHP_FUNCTION( magickgetversionstring );
/* end convenience functions */

	PHP_FUNCTION( magickhasnextimage );
	PHP_FUNCTION( magickhaspreviousimage );
	PHP_FUNCTION( magickimplodeimage );
	PHP_FUNCTION( magicklabelimage );

	PHP_FUNCTION( magicklevelimage );
/*	PHP_FUNCTION( magicklevelimagechannel ); */

	PHP_FUNCTION( magickmagnifyimage );
	PHP_FUNCTION( magickmapimage );
	PHP_FUNCTION( magickmattefloodfillimage );
	PHP_FUNCTION( magickmedianfilterimage );
	PHP_FUNCTION( magickminifyimage );
	PHP_FUNCTION( magickmodulateimage );
	PHP_FUNCTION( magickmontageimage );
	PHP_FUNCTION( magickmorphimages );
	PHP_FUNCTION( magickmosaicimages );
	PHP_FUNCTION( magickmotionblurimage );

	PHP_FUNCTION( magicknegateimage );
/*	PHP_FUNCTION( magicknegateimagechannel ); */

	PHP_FUNCTION( magicknewimage );

	PHP_FUNCTION( magicknextimage );
	PHP_FUNCTION( magicknormalizeimage );
	PHP_FUNCTION( magickoilpaintimage );
	PHP_FUNCTION( magickorderedposterizeimage );

	PHP_FUNCTION( magickfloodfillpaintimage );
	PHP_FUNCTION( magickopaquepaintimage );
	PHP_FUNCTION( magicktransparentpaintimage );

	PHP_FUNCTION( magickpingimage );
	PHP_FUNCTION( magickposterizeimage );
	PHP_FUNCTION( magickpreviewimages );
	PHP_FUNCTION( magickpreviousimage );
	PHP_FUNCTION( magickprofileimage );
	PHP_FUNCTION( magickquantizeimage );
	PHP_FUNCTION( magickquantizeimages );
	PHP_FUNCTION( magickqueryconfigureoption );
	PHP_FUNCTION( magickqueryconfigureoptions );
	PHP_FUNCTION( magickqueryfontmetrics );

/* the following functions are convenience functions I implemented, as alternatives to calling MagickQueryFontMetrics */
	PHP_FUNCTION( magickgetcharwidth );
	PHP_FUNCTION( magickgetcharheight );
	PHP_FUNCTION( magickgettextascent );
	PHP_FUNCTION( magickgettextdescent );
	PHP_FUNCTION( magickgetstringwidth );
	PHP_FUNCTION( magickgetstringheight );
	PHP_FUNCTION( magickgetmaxtextadvance );
/* end of convenience functions */

	PHP_FUNCTION( magickqueryfonts );
	PHP_FUNCTION( magickqueryformats );
	PHP_FUNCTION( magickradialblurimage );
	PHP_FUNCTION( magickraiseimage );
	PHP_FUNCTION( magickreadimage );
	PHP_FUNCTION( magickreadimageblob );
	PHP_FUNCTION( magickreadimagefile );

	/* Custom PHP function; accepts a PHP array of filenames and attempts to read them all into the MagickWand */
	PHP_FUNCTION( magickreadimages );

	PHP_FUNCTION( magickrecolorimage );
	PHP_FUNCTION( magickreducenoiseimage );
	PHP_FUNCTION( magickremoveimage );
	PHP_FUNCTION( magickremoveimageprofile );

	PHP_FUNCTION( magickremoveimageprofiles );

	PHP_FUNCTION( magickresetimagepage );
	PHP_FUNCTION( magickresetiterator );
	PHP_FUNCTION( magickresampleimage );
	PHP_FUNCTION( magickresizeimage );
	PHP_FUNCTION( magickrollimage );
	PHP_FUNCTION( magickrotateimage );
	PHP_FUNCTION( magicksampleimage );
	PHP_FUNCTION( magickscaleimage );
	PHP_FUNCTION( magickseparateimagechannel );

	PHP_FUNCTION( magicksepiatoneimage );

	PHP_FUNCTION( magicksetbackgroundcolor );

	PHP_FUNCTION( magicksetcompression );

	PHP_FUNCTION( magicksetcompressionquality );
	PHP_FUNCTION( magicksetfilename );
	PHP_FUNCTION( magicksetfirstiterator );
	PHP_FUNCTION( magicksetformat );

	PHP_FUNCTION( magicksetimage );

	PHP_FUNCTION( magickgetimagecompose );
	PHP_FUNCTION( magicksetimagealphachannel );
	PHP_FUNCTION( magicksetimageattribute );

	PHP_FUNCTION( magicksetimagebackgroundcolor );
	PHP_FUNCTION( magicksetimagebias );
	PHP_FUNCTION( magicksetimageblueprimary );
	PHP_FUNCTION( magicksetimagebordercolor );
	PHP_FUNCTION( magicksetimageclipmask );
	PHP_FUNCTION( magicksetimagecolormapcolor );
	PHP_FUNCTION( magicksetimagecolorspace );
	PHP_FUNCTION( magicksetimagecompose );
	PHP_FUNCTION( magicksetimagecompression );
	PHP_FUNCTION( magicksetimagecompressionquality );
	PHP_FUNCTION( magicksetimagedelay );

	PHP_FUNCTION( magicksetimagedepth );
/*	PHP_FUNCTION( magicksetimagechanneldepth ); */

	PHP_FUNCTION( magicksetimagedispose );
	PHP_FUNCTION( magicksetimageendian );

	PHP_FUNCTION( magicksetimageextent );

	PHP_FUNCTION( magicksetimagefilename );

	PHP_FUNCTION( magicksetimageformat );

	PHP_FUNCTION( magicksetimagegamma );
	PHP_FUNCTION( magicksetimagegreenprimary );
	PHP_FUNCTION( magicksetimageindex );
	PHP_FUNCTION( magicksetimageinterlacescheme );
	PHP_FUNCTION( magicksetimageiterations );
	PHP_FUNCTION( magicksetimagemattecolor );
	PHP_FUNCTION( magicksetimageopacity );
	PHP_FUNCTION( magicksetimageoption );

	PHP_FUNCTION( magicksetimagepage );

	PHP_FUNCTION( magicksetimagepixels );
	PHP_FUNCTION( magicksetimageproperty );
	PHP_FUNCTION( magicksetimageprofile );
	PHP_FUNCTION( magicksetimageredprimary );
	PHP_FUNCTION( magicksetimagerenderingintent );
	PHP_FUNCTION( magicksetimageresolution );
	PHP_FUNCTION( magicksetimagescene );

	PHP_FUNCTION( magicksetimagetickspersecond );

	PHP_FUNCTION( magicksetimagetype );
	PHP_FUNCTION( magicksetimageunits );
	PHP_FUNCTION( magicksetimagevirtualpixelmethod );
	PHP_FUNCTION( magicksetimagewhitepoint );

	PHP_FUNCTION( magicksetinterlacescheme );
	PHP_FUNCTION( magicksetlastiterator );

	PHP_FUNCTION( magicksetoption );

	PHP_FUNCTION( magicksetpage );

	PHP_FUNCTION( magicksetpassphrase );
	PHP_FUNCTION( magicksetresolution );
	PHP_FUNCTION( magicksetresourcelimit );
	PHP_FUNCTION( magicksetsamplingfactors );

	PHP_FUNCTION( magicksetsize );

	PHP_FUNCTION( magicksettype );

	PHP_FUNCTION( magickshadowimage );

	PHP_FUNCTION( magicksharpenimage );
/*	PHP_FUNCTION( magicksharpenimagechannel ); */

	PHP_FUNCTION( magickshaveimage );
	PHP_FUNCTION( magickshearimage );
	PHP_FUNCTION( magicksolarizeimage );
	PHP_FUNCTION( magickspliceimage );
	PHP_FUNCTION( magickspreadimage );
	PHP_FUNCTION( magickstatisticimage );
	PHP_FUNCTION( magicksteganoimage );
	PHP_FUNCTION( magickstereoimage );
	PHP_FUNCTION( magickstripimage );
	PHP_FUNCTION( magickswirlimage );
	PHP_FUNCTION( magicktextureimage );

	PHP_FUNCTION( magickthresholdimage );
/*	PHP_FUNCTION( magickthresholdimagechannel ); */

	PHP_FUNCTION( magickthumbnailimage );

	PHP_FUNCTION( magicktintimage );
	PHP_FUNCTION( magicktransformimage );
	PHP_FUNCTION( magicktrimimage );

	PHP_FUNCTION( magickunsharpmaskimage );
/*	PHP_FUNCTION( magickunsharpmaskimagechannel ); */

	PHP_FUNCTION( magickwaveimage );
	PHP_FUNCTION( magickwhitethresholdimage );
	PHP_FUNCTION( magickwriteimage );
	PHP_FUNCTION( magickwriteimagefile );
	PHP_FUNCTION( magickwriteimages );
	PHP_FUNCTION( newmagickwand );

	/* PixelIterator functions */

	PHP_FUNCTION( clearpixeliterator );
	PHP_FUNCTION( destroypixeliterator );
	PHP_FUNCTION( newpixeliterator );
	PHP_FUNCTION( newpixelregioniterator );

	PHP_FUNCTION( pixelgetiteratorexception );
	PHP_FUNCTION( pixelgetiteratorexceptionstring );
	PHP_FUNCTION( pixelgetiteratorexceptiontype );

	PHP_FUNCTION( pixelgetnextiteratorrow );

	PHP_FUNCTION( pixelgetpreviousiteratorrow );

	PHP_FUNCTION( pixelresetiterator );

	PHP_FUNCTION( pixelsetfirstiteratorrow );

	PHP_FUNCTION( pixelsetiteratorrow );

	PHP_FUNCTION( pixelsetlastiteratorrow );

	PHP_FUNCTION( pixelsynciterator );

	/* PixelWand functions */

	PHP_FUNCTION( clearpixelwand );
	
	PHP_FUNCTION( clonepixelwand );

	PHP_FUNCTION( destroypixelwand );

/************************** OW_Modified *************************
MagickWand's destroypixelwandarray() == ImageMagick's destroypixelwands
*/
/*	PHP_FUNCTION( destroypixelwands ); */
	PHP_FUNCTION( destroypixelwandarray );

	PHP_FUNCTION( ispixelwandsimilar );

	PHP_FUNCTION( newpixelwand );

/************************** OW_Modified *************************
MagickWand's newpixelwandarray() == ImageMagick's newpixelwands
*/
/*	PHP_FUNCTION( newpixelwands ); */
	PHP_FUNCTION( newpixelwandarray );

	PHP_FUNCTION( pixelgetalpha );
	PHP_FUNCTION( pixelgetalphaquantum );

	PHP_FUNCTION( pixelgetexception );
	PHP_FUNCTION( pixelgetexceptionstring );
	PHP_FUNCTION( pixelgetexceptiontype );

	PHP_FUNCTION( pixelgetblack );
	PHP_FUNCTION( pixelgetblackquantum );
	PHP_FUNCTION( pixelgetblue );
	PHP_FUNCTION( pixelgetbluequantum );
	PHP_FUNCTION( pixelgetcolorasstring );
	PHP_FUNCTION( pixelgetcolorcount );
	PHP_FUNCTION( pixelgetcyan );
	PHP_FUNCTION( pixelgetcyanquantum );
	PHP_FUNCTION( pixelgetgreen );
	PHP_FUNCTION( pixelgetgreenquantum );
	PHP_FUNCTION( pixelgetindex );
	PHP_FUNCTION( pixelgetmagenta );
	PHP_FUNCTION( pixelgetmagentaquantum );
	PHP_FUNCTION( pixelgetopacity );
	PHP_FUNCTION( pixelgetopacityquantum );
	PHP_FUNCTION( pixelgetquantumcolor );
	PHP_FUNCTION( pixelgetred );
	PHP_FUNCTION( pixelgetredquantum );
	PHP_FUNCTION( pixelgetyellow );
	PHP_FUNCTION( pixelgetyellowquantum );
	PHP_FUNCTION( pixelsetalpha );
	PHP_FUNCTION( pixelsetalphaquantum );
	PHP_FUNCTION( pixelsetblack );
	PHP_FUNCTION( pixelsetblackquantum );
	PHP_FUNCTION( pixelsetblue );
	PHP_FUNCTION( pixelsetbluequantum );
	PHP_FUNCTION( pixelsetcolor );
	PHP_FUNCTION( pixelsetcolorcount );
	PHP_FUNCTION( pixelsetcyan );
	PHP_FUNCTION( pixelsetcyanquantum );
	PHP_FUNCTION( pixelsetgreen );
	PHP_FUNCTION( pixelsetgreenquantum );
	PHP_FUNCTION( pixelsetindex );
	PHP_FUNCTION( pixelsetmagenta );
	PHP_FUNCTION( pixelsetmagentaquantum );
	PHP_FUNCTION( pixelsetopacity );
	PHP_FUNCTION( pixelsetopacityquantum );
	PHP_FUNCTION( pixelsetquantumcolor );
	PHP_FUNCTION( pixelsetred );
	PHP_FUNCTION( pixelsetredquantum );
	PHP_FUNCTION( pixelsetyellow );
	PHP_FUNCTION( pixelsetyellowquantum );

#ifdef ZTS
#define MAGICKWAND_G(v) TSRMG(magickwand_globals_id, zend_magickwand_globals *, v)
#else
#define MAGICKWAND_G(v) (magickwand_globals.v)
#endif

#endif /* PHP_MAGICKWAND_H */
