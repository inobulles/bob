// much of this mimics the behaviour of the old aqua-manager tool:
// https://github.com/inobulles/aqua-manager

#include <util.h>

#include <classes/package.h>
#include <classes/package_t.h>

#include <pwd.h>
#include <time.h>
#include <unistd.h>

// foreign method binding

WrenForeignMethodFn package_bind_foreign_method(bool static_, char const* signature) {
	// getters

	BIND_FOREIGN_METHOD(false, "unique()", package_get_unique)
	BIND_FOREIGN_METHOD(false, "name()", package_get_name)
	BIND_FOREIGN_METHOD(false, "description()", package_get_description)
	BIND_FOREIGN_METHOD(false, "version()", package_get_version)
	BIND_FOREIGN_METHOD(false, "author()", package_get_author)
	BIND_FOREIGN_METHOD(false, "organization()", package_get_organization)
	BIND_FOREIGN_METHOD(false, "www()", package_get_www)

	// setters

	BIND_FOREIGN_METHOD(false, "unique=(_)", package_set_unique)
	BIND_FOREIGN_METHOD(false, "name=(_)", package_set_name)
	BIND_FOREIGN_METHOD(false, "description=(_)", package_set_description)
	BIND_FOREIGN_METHOD(false, "version=(_)", package_set_version)
	BIND_FOREIGN_METHOD(false, "author=(_)", package_set_author)
	BIND_FOREIGN_METHOD(false, "organization=(_)", package_set_organization)
	BIND_FOREIGN_METHOD(false, "www=(_)", package_set_www)

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

	// defaults

	package->name = strdup("Untitled Project");
	package->description = strdup("Untitled project which has no title");
	package->version = strdup("0.69.420");
	package->www = strdup("https://youtu.be/dQw4w9WgXcQ");

	// attempt to set the author to the username

	uid_t const uid = getuid();
	struct passwd* const passwd = getpwuid(uid);

	package->author = strdup(passwd ?
		passwd->pw_name :
		"Anonymousia de Bergerac-Fleur");

	// attempt to set the organization to the hostname

	size_t const len = sysconf(_SC_HOST_NAME_MAX);
	package->organization = calloc(1, len + 1);

	if (gethostname(package->organization, len) < 0) {
		free(package->organization);
		package->organization = strdup("Knights of Can-A-Lot");
	}

	// from all this info, create a completely random unique value

	time_t const seconds = time(NULL);
	srand(seconds ^ hash_str(package->organization) ^ hash_str(package->author) ^ hash_str(package->name));
	if (asprintf(&package->unique, "%lx:%x", seconds, rand())) {}
}

void package_del(void* _package) {
	package_t* const package = _package;

	strfree(&package->entry);
	strfree(&package->name);
	strfree(&package->description);
	strfree(&package->version);
	strfree(&package->author);
	strfree(&package->organization);
	strfree(&package->www);
}

// getters

#define GETTER(name) \
	void package_get_##name(WrenVM* vm) { \
		CHECK_ARGC("Package." #name, 0, 0) \
		\
		package_t* const package = foreign; \
		wrenSetSlotString(vm, 0, package->name); \
	}

GETTER(unique)
GETTER(name)
GETTER(description)
GETTER(version)
GETTER(author)
GETTER(organization)
GETTER(www)

// setters

#define SETTER(name) \
	void package_set_##name(WrenVM* vm) { \
		CHECK_ARGC("Package." #name "=", 1, 1) \
		\
		ASSERT_ARG_TYPE(1, WREN_TYPE_STRING) \
		\
		package_t* const package = foreign; \
		char const* const name = wrenGetSlotString(vm, 1); \
		\
		strfree(&package->name); \
		package->name = strdup(name); \
	}

SETTER(unique)
SETTER(name)
SETTER(description)
SETTER(version)
SETTER(author)
SETTER(organization)
SETTER(www)
