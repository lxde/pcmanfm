#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(3, 0, 0)

static void pcmanfm_apply_modern_ui_css(void)
{
    GtkCssProvider *provider;
    GdkScreen *screen;
    GError *error = NULL;
    char *css_path;

    provider = gtk_css_provider_new();
    css_path = g_build_filename(PACKAGE_DATA_DIR, "pcmanfm.css", NULL);

    if (!gtk_css_provider_load_from_path(provider, css_path, &error))
    {
        g_warning("Could not load PCManFM modern UI stylesheet %s: %s",
                  css_path, error ? error->message : "unknown error");
        g_clear_error(&error);
    }
    else
    {
        screen = gdk_screen_get_default();
        if (screen)
            gtk_style_context_add_provider_for_screen(
                screen,
                GTK_STYLE_PROVIDER(provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    g_free(css_path);
    g_object_unref(provider);
}

#ifdef __GNUC__
__attribute__((constructor))
#endif
static void pcmanfm_modern_ui_constructor(void)
{
    pcmanfm_apply_modern_ui_css();
}

#endif /* GTK3 */
