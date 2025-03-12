#include <stdio.h>
#include <string>
#include <filesystem>

struct log_files
{
	std::string file_name;
  FILE *result;
  FILE *history;

  void init(std::string path_str){
    std::string file_name = std::filesystem::path(path_str).filename();
    file_name = file_name.substr(0,file_name.size()-3);
    std::filesystem::path output_path = std::filesystem::path("output")/file_name;
    std::filesystem::create_directories(output_path);
    output_path /= file_name;
    std::string result_filename = output_path.string()+"_result.log";
    std::string history_filename = output_path.string()+"_history.log";
    result = fopen(result_filename.c_str(),"w");
    history = fopen(history_filename.c_str(),"w");
    printf("Result log file: %s\n", result_filename.c_str());
    printf("History log file: %s\n", history_filename.c_str());
  }

  ~log_files(){
    fclose(result);
    fclose(history);
  }
};
