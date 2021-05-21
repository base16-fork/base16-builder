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
clone(std::string dir, std::string source)
{
	std::ifstream file(source);
	if (file.is_open()) {
		YAML::Node file = YAML::LoadFile(source);

		std::vector<std::string> token_key;
		std::vector<std::string> token_value;

		for (YAML::const_iterator it = file.begin(); it != file.end(); ++it) {
			token_key.push_back(it->first.as<std::string>());
			token_value.push_back(it->second.as<std::string>());
		}

		for (int i = 0; i < token_key.size(); ++i) {
			do_clone((dir + token_key[i]).c_str(), token_value[i].c_str());
		}
	} else {
		std::cerr << "error: cannot read " << source << std::endl;
		exit(1);
	}
	file.close();
}

void
emit_source(void) {
	std::ofstream file("sources.yaml");

	if (file.is_open()) {
		YAML::Emitter source;
		source << YAML::BeginMap;
		source << YAML::Key << "schemes";
		source << YAML::Value << "https://github.com/chriskempson/base16-schemes-source.git";
		source << YAML::Key << "templates";
		source << YAML::Value << "https://github.com/chriskempson/base16-templates-source.git";
		source << YAML::EndMap;

		file << source.c_str();
	} else {
		std::cerr << "error: fail to write source.yaml to current directory";
		exit(1);
	}
	file.close();
}

int
main(int argc, char *argv[])
{
	emit_source();
	clone("./sources/", "sources.yaml");
	clone("./schemes/", "sources/schemes/list.yaml");
	clone("./templates/", "sources/templates/list.yaml");

	return 0;
}
