// glgrab: zrzut klatki prosto z bufora OpenGL gry uruchomionej pod Wine (laboratorium, tools/lab/README.md).
//
// Ładowana przez LD_PRELOAD. Wine 11 rysuje OpenGL przez EGL (win32u.so otwiera libEGL przez dlopen i pobiera
// funkcje przez dlsym albo eglGetProcAddress); starsze ścieżki GLX (winex11.so, libGL.so.1) też obsługujemy.
// Przechwytujemy dlsym i eglGetProcAddress: na prośbę o eglSwapBuffers* / glXSwapBuffers* oddajemy własne
// funkcje. Przed pokazaniem klatki sprawdzamy, czy istnieje plik prośby $OW_GRAB_DIR/grab.request; jeśli tak,
// czytamy tylny bufor domyślnego framebuffera (glReadPixels), zapisujemy PPM pod ścieżką podaną w prośbie
// (najpierw .tmp, potem rename) i kasujemy prośbę. Stan OpenGL gry odtwarzamy po odczycie.
// Nie zaglądamy w kod gry: przechwytujemy standardową funkcję systemowej biblioteki OpenGL.
//
// Budowanie: gcc -shared -fPIC -O2 -o glgrab.so glgrab.c -ldl
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef void* (*dlsym_t)(void*, const char*);
typedef void (*swap_t)(void*, unsigned long);
typedef int64_t (*swapoml_t)(void*, unsigned long, int64_t, int64_t, int64_t);
typedef void* (*getproc_t)(const unsigned char*);
typedef unsigned (*eglswap_t)(void*, void*);
typedef unsigned (*eglswapdmg_t)(void*, void*, const int*, int);
typedef void* (*eglgetproc_t)(const char*);
typedef unsigned (*eglquery_t)(void*, void*, int, int*);

#define GL_VIEWPORT 0x0BA2
#define GL_BACK 0x0405
#define GL_READ_BUFFER 0x0C02
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_PACK_ROW_LENGTH 0x0D02
#define GL_RGB 0x1907
#define GL_UNSIGNED_BYTE 0x1401
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#define GLX_WIDTH 0x801D
#define GLX_HEIGHT 0x801E

static dlsym_t real_dlsym;
static swap_t real_swap;
static swapoml_t real_swap_oml;
static getproc_t getproc;
static eglgetproc_t egl_getproc;
static eglswap_t real_egl_swap;
static eglswapdmg_t real_egl_swap_khr, real_egl_swap_ext;
static eglquery_t egl_query;

static dlsym_t get_real_dlsym(void)
{
	if (!real_dlsym)
	{
		real_dlsym = (dlsym_t)dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.34");
		if (!real_dlsym) real_dlsym = (dlsym_t)dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
	}
	return real_dlsym;
}

static void* gl(const char* name)
{
	if (egl_getproc) return egl_getproc(name);
	return getproc ? getproc((const unsigned char*)name) : NULL;
}

// Rozmiar powierzchni: EGL (egl != 0) albo GLX; 0 = z glViewport.
static void grab(void* dpy, unsigned long drawable, int egl)
{
	const char* dir = getenv("OW_GRAB_DIR");
	if (!dir || (!getproc && !egl_getproc)) return;
	char req[1024], out[1024], tmp[1100];
	snprintf(req, sizeof req, "%s/grab.request", dir);
	if (access(req, F_OK) != 0) return;
	FILE* f = fopen(req, "r");
	if (!f) return;
	if (!fgets(out, sizeof out, f)) out[0] = 0;
	fclose(f);
	out[strcspn(out, "\r\n")] = 0;
	unlink(req);
	if (!out[0]) return;

	void (*GetIntegerv)(unsigned, int*) = gl("glGetIntegerv");
	void (*ReadBuffer)(unsigned) = gl("glReadBuffer");
	void (*PixelStorei)(unsigned, int) = gl("glPixelStorei");
	void (*ReadPixels)(int, int, int, int, unsigned, unsigned, void*) = gl("glReadPixels");
	void (*BindFramebuffer)(unsigned, unsigned) = gl("glBindFramebuffer");
	void (*BindBuffer)(unsigned, unsigned) = gl("glBindBuffer");
	void (*QueryDrawable)(void*, unsigned long, int, unsigned*) = gl("glXQueryDrawable");
	if (!GetIntegerv || !ReadBuffer || !PixelStorei || !ReadPixels) return;

	unsigned w = 0, h = 0;
	if (egl && egl_query)
	{
		int ew = 0, eh = 0;
		egl_query(dpy, (void*)drawable, 0x3057 /* EGL_WIDTH */, &ew);
		egl_query(dpy, (void*)drawable, 0x3056 /* EGL_HEIGHT */, &eh);
		w = (unsigned)ew, h = (unsigned)eh;
	}
	else if (QueryDrawable)
	{
		QueryDrawable(dpy, drawable, GLX_WIDTH, &w);
		QueryDrawable(dpy, drawable, GLX_HEIGHT, &h);
	}
	if (!w || !h)
	{
		int vp[4] = {0};
		GetIntegerv(GL_VIEWPORT, vp);
		w = (unsigned)vp[2], h = (unsigned)vp[3];
	}
	if (!w || !h || w > 16384 || h > 16384) return;

	// stan gry, który zmieniamy
	int fbo = 0, pbo = 0, rbuf = 0, align = 4, rowlen = 0;
	if (BindFramebuffer) GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &fbo);
	if (BindBuffer) GetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
	GetIntegerv(GL_READ_BUFFER, &rbuf);
	GetIntegerv(GL_PACK_ALIGNMENT, &align);
	GetIntegerv(GL_PACK_ROW_LENGTH, &rowlen);

	if (BindFramebuffer) BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	if (BindBuffer) BindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	ReadBuffer(GL_BACK);
	PixelStorei(GL_PACK_ALIGNMENT, 1);
	PixelStorei(GL_PACK_ROW_LENGTH, 0);
	unsigned char* px = malloc((size_t)w * h * 3);
	if (px) ReadPixels(0, 0, (int)w, (int)h, GL_RGB, GL_UNSIGNED_BYTE, px);

	ReadBuffer((unsigned)rbuf);
	PixelStorei(GL_PACK_ALIGNMENT, align);
	PixelStorei(GL_PACK_ROW_LENGTH, rowlen);
	if (BindBuffer) BindBuffer(GL_PIXEL_PACK_BUFFER, (unsigned)pbo);
	if (BindFramebuffer) BindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned)fbo);
	if (!px) return;

	// PPM, wiersz 0 = góra obrazu (OpenGL czyta od dołu)
	snprintf(tmp, sizeof tmp, "%s.tmp", out);
	f = fopen(tmp, "wb");
	if (f)
	{
		fprintf(f, "P6\n%u %u\n255\n", w, h);
		for (unsigned y = 0; y < h; ++y) fwrite(px + (size_t)(h - 1 - y) * w * 3, 1, (size_t)w * 3, f);
		fclose(f);
		rename(tmp, out);
	}
	free(px);
}

static void hook_swap(void* dpy, unsigned long drawable)
{
	grab(dpy, drawable, 0);
	real_swap(dpy, drawable);
}

static int64_t hook_swap_oml(void* dpy, unsigned long drawable, int64_t a, int64_t b, int64_t c)
{
	grab(dpy, drawable, 0);
	return real_swap_oml(dpy, drawable, a, b, c);
}

static unsigned hook_egl_swap(void* dpy, void* surf)
{
	grab(dpy, (unsigned long)surf, 1);
	return real_egl_swap(dpy, surf);
}
static unsigned hook_egl_swap_khr(void* dpy, void* surf, const int* r, int n)
{
	grab(dpy, (unsigned long)surf, 1);
	return real_egl_swap_khr(dpy, surf, r, n);
}
static unsigned hook_egl_swap_ext(void* dpy, void* surf, const int* r, int n)
{
	grab(dpy, (unsigned long)surf, 1);
	return real_egl_swap_ext(dpy, surf, r, n);
}

// Podmiana funkcji EGL (wspólna dla dlsym i eglGetProcAddress).
static void* egl_hook(const char* name, void* p)
{
	if (!p) return p;
	if (!strcmp(name, "eglQuerySurface")) egl_query = (eglquery_t)p;
	if (!strcmp(name, "eglSwapBuffers"))
	{
		real_egl_swap = (eglswap_t)p;
		return (void*)hook_egl_swap;
	}
	if (!strcmp(name, "eglSwapBuffersWithDamageKHR"))
	{
		real_egl_swap_khr = (eglswapdmg_t)p;
		return (void*)hook_egl_swap_khr;
	}
	if (!strcmp(name, "eglSwapBuffersWithDamageEXT"))
	{
		real_egl_swap_ext = (eglswapdmg_t)p;
		return (void*)hook_egl_swap_ext;
	}
	return p;
}

static eglgetproc_t real_egl_getproc;
static void* hook_egl_getproc(const char* name)
{
	return egl_hook(name, real_egl_getproc(name));
}

void* dlsym(void* handle, const char* name)
{
	dlsym_t rd = get_real_dlsym();
	void* p = rd(handle, name);
	if (!p) return p;
	if (!strcmp(name, "eglGetProcAddress"))
	{
		real_egl_getproc = egl_getproc = (eglgetproc_t)p;
		return (void*)hook_egl_getproc;
	}
	if (!strncmp(name, "egl", 3)) return egl_hook(name, p);
	if (!getproc && (!strcmp(name, "glXGetProcAddressARB") || !strcmp(name, "glXGetProcAddress"))) getproc = (getproc_t)p;
	if (!strcmp(name, "glXSwapBuffers"))
	{
		real_swap = (swap_t)p;
		if (!getproc) getproc = (getproc_t)rd(handle, "glXGetProcAddressARB");
		return (void*)hook_swap;
	}
	if (!strcmp(name, "glXSwapBuffersMscOML"))
	{
		real_swap_oml = (swapoml_t)p;
		return (void*)hook_swap_oml;
	}
	return p;
}
