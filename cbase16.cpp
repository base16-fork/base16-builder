#include <git2.h>
#include <iostream>

int
do_clone(const char *path, const char *url)
{
	git_libgit2_init();
	git_repository *repo = NULL;
	int ret = git_clone(&repo, url, path, NULL);
	git_repository_free(repo);
	return ret;
}

int
main(int argc, char *argv[])
{
	return 0;
}
