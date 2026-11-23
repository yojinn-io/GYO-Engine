/*
 * The FreeType modules the text Module uses (FT_CONFIG_MODULES_H): sfnt with
 * the TrueType and CFF drivers, their PostScript helpers, the auto-hinter and
 * the anti-aliasing rasterizer. Nothing else is compiled (decision
 * v8-arch-gyo2-sdl-t, docs/architecture/target/v2/text-declaration.zh-Hant.md
 * section 5.1).
 */
FT_USE_MODULE( FT_Module_Class, autofit_module_class )
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, cff_driver_class )
FT_USE_MODULE( FT_Module_Class, psaux_module_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, pshinter_module_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )
