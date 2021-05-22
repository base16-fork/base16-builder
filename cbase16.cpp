#include <git2.h>
#include <yaml-cpp/yaml.h>

#include <vector>
#include <fstream>
#include <iostream>

int
fetch_progress(const git_transfer_progress *stats, void *payload)
{
	int fetch_percent =
		stats->received_objects / stats->total_objects * 100;
	int index_percent = stats->indexed_objects / stats->total_objects * 100;
	int kbytes = stats->received_bytes / 1024;

	printf("\rFetch progress: network %d%% (%d kb, %d/%d) / index %d%% (%d/%d)",
	       fetch_percent, kbytes, stats->received_objects,
	       stats->total_objects, index_percent, stats->indexed_objects,
	       stats->total_objects);

	fflush(stdout);

	if (index_percent == 100) {
		std::cout << ", done." << std::endl;
		fflush(stdout);
	}

	return 0;
}

void
checkout_progress(const char *path, std::size_t cur, std::size_t tot,
                  void *payload)
{
	int percent = cur / tot * 100;
	printf("\rCheckout progress: %d%% (%zu/%zu)", percent, cur, tot);
	fflush(stdout);

	if (percent == 100) {
		std::cout << ", done." << std::endl;
		fflush(stdout);
	}
}

int
do_clone(const char *path, const char *url)
{
	git_libgit2_init();

	git_repository *repo = NULL;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	opts.checkout_opts.progress_cb = checkout_progress;
	opts.fetch_opts.callbacks.transfer_progress = fetch_progress;

	int ret = git_clone(&repo, url, path, &opts);
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

		for (YAML::const_iterator it = file.begin(); it != file.end();
		     ++it) {
			token_key.push_back(it->first.as<std::string>());
			token_value.push_back(it->second.as<std::string>());
		}

		#pragma omp parallel for
		for (int i = 0; i < token_key.size(); ++i) {
			printf("Cloning %s (%s):\n", token_key[i].c_str(),
			       token_value[i].c_str());
			do_clone((dir + token_key[i]).c_str(),
			         token_value[i].c_str());
			std::cout << std::endl;
		}
	} else {
		std::cerr << "error: cannot read " << source << std::endl;
		exit(1);
	}
	file.close();
}

void
emit_source(void)
{
	std::ofstream file("sources.yaml");

	if (file.is_open()) {
		YAML::Emitter source;
		source << YAML::BeginMap;
		source << YAML::Key << "schemes";
		source << YAML::Value
		       << "https://github.com/chriskempson/base16-schemes-source.git";
		source << YAML::Key << "templates";
		source << YAML::Value
		       << "https://github.com/chriskempson/base16-templates-source.git";
		source << YAML::EndMap;

		file << source.c_str();
	} else {
		std::cerr
			<< "error: fail to write source.yaml to current directory";
		exit(1);
	}
	file.close();
}

int
main(int argc, char *argv[])
{
	emit_source();
	clone("./sources/", "sources.yaml");

	#pragma omp parallel sections
	{
		#pragma omp section
		{
			clone("./schemes/", "sources/schemes/list.yaml");
		}
		#pragma omp section
		{
			clone("./templates/", "sources/templates/list.yaml");
		}
	}

	return 0;
}
