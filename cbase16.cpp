#include <git2.h>
#include <yaml-cpp/yaml.h>

#include <vector>
#include <fstream>
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

void
clone(std::string source)
{
	std::ifstream file(source);
	if (file.good()) {
		YAML::Node file = YAML::LoadFile(source);

		std::vector<std::string> token_key;
		std::vector<std::string> token_value;

		for (YAML::const_iterator it = file.begin(); it != file.end(); ++it) {
			token_key.push_back(it->first.as<std::string>());
			token_value.push_back(it->second.as<std::string>());
		}

		for (int i = 0; i < token_key.size(); ++i) {
			do_clone(token_key[i].c_str(), token_value[i].c_str());
		}
	} else {
		std::cerr << "error: cannot read " << source << ".yaml" << std::endl;
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	return 0;
}
