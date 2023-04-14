#include <util.h>
#include <classes/package.h>

typedef struct {
	char* entry;
	char* name;
	char* descr;
	char* vers;
	char* author;
	char* org;
	char* www;
	char* icon;
} package_t;

// foreign method binding

WrenForeignMethodFn package_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "name()", package_get_name)
	BIND_FOREIGN_METHOD(false, "descr()", package_get_description)
	BIND_FOREIGN_METHOD(false, "vers()", package_get_version)
	BIND_FOREIGN_METHOD(false, "author()", package_get_author)
	BIND_FOREIGN_METHOD(false, "org()", package_get_organization)
	BIND_FOREIGN_METHOD(false, "www()", package_get_www)
	BIND_FOREIGN_METHOD(false, "icon()", package_get_icon)

	// setters

	BIND_FOREIGN_METHOD(false, "name=(_)", package_set_name)
	BIND_FOREIGN_METHOD(false, "descr=(_)", package_set_description)
	BIND_FOREIGN_METHOD(false, "vers=(_)", package_set_version)
	BIND_FOREIGN_METHOD(false, "author=(_)", package_set_author)
	BIND_FOREIGN_METHOD(false, "org=(_)", package_set_organization)
	BIND_FOREIGN_METHOD(false, "www=(_)", package_set_www)
	BIND_FOREIGN_METHOD(false, "icon=(_)", package_set_icon)

	// unknown

	return wren_unknown_foreign;
}

// constructor/destructor

void package_new(WrenVM* vm) {
	CHECK_ARGC("Package.new", 1, 1)

	ASSERT_ARG_TYPE(1, WREN_TYPE_STRING)

	package_t* const package = wrenSetSlotNewForeign(vm, 0, 0, sizeof *package);
	char const* const entry = wrenGetSlotString(vm, 1);

	package->entry = strdup(entry);
}

void package_del(void* _package) {
	(void) _package;
}

// getters

void package_get_name(WrenVM* vm){ (void) vm; }
void package_get_description(WrenVM* vm){ (void) vm; }
void package_get_version(WrenVM* vm){ (void) vm; }
void package_get_author(WrenVM* vm){ (void) vm; }
void package_get_organization(WrenVM* vm){ (void) vm; }
void package_get_www(WrenVM* vm){ (void) vm; }
void package_get_icon(WrenVM* vm){ (void) vm; }

// setters

void package_set_name(WrenVM* vm){ (void) vm; }
void package_set_description(WrenVM* vm){ (void) vm; }
void package_set_version(WrenVM* vm){ (void) vm; }
void package_set_author(WrenVM* vm){ (void) vm; }
void package_set_organization(WrenVM* vm){ (void) vm; }
void package_set_www(WrenVM* vm){ (void) vm; }
void package_set_icon(WrenVM* vm){ (void) vm; }
