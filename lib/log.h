#include <stdio.h>
#include <string>
#include <filesystem>

extern bool LOAD_DEPENDENT_BRANCHES;
extern int U_incrment;

struct log_files
{
    std::string file_name;
    FILE *result;
    FILE *history;
    FILE *pred_history;
    FILE *CyclWP_summary;

    void init(std::string path_str){
        std::string file_name = std::filesystem::path(path_str).filename();
        file_name = file_name.substr(0, file_name.size() - 3);
        
        // Create additional directory level based on parameter values
        std::string sub_dir = (LOAD_DEPENDENT_BRANCHES ? "LDB_Enabled" : "LDB_Disabled") + ("_U_" + std::to_string(U_incrment));
        
        std::filesystem::path output_path = std::filesystem::path("output") / sub_dir / file_name;
        std::filesystem::create_directories(output_path);
        output_path /= file_name;
        
        std::string result_filename = output_path.string() + "_result.log";
        std::string history_filename = output_path.string() + "_history.log";
        std::string pred_history_filename = output_path.string() + "_pred_history.log";
        std::string CyclWP_summary_filename = output_path.string() + "_CyclWP_summary.log";
        
        result = fopen(result_filename.c_str(), "w");
        history = fopen(history_filename.c_str(), "w");
        pred_history = fopen(pred_history_filename.c_str(), "w");
        CyclWP_summary = fopen(CyclWP_summary_filename.c_str(), "w");
        
        // Redirect stdout to the result file
        if (freopen(result_filename.c_str(), "w", stdout) == nullptr) {
          std::cerr << "Error redirecting stdout to result log file." << std::endl;
          fclose(result); // Close the file if redirection fails
          return; // Handle the error as needed
      }
      
        printf("Result log file: %s\n", result_filename.c_str());
        printf("History log file: %s\n", history_filename.c_str());
        printf("Pred History log file: %s\n", pred_history_filename.c_str());
        printf("CyclWP summary log file: %s\n", CyclWP_summary_filename.c_str());
    }

    ~log_files(){
        fclose(result);
        fclose(history);
        fclose(pred_history);
        fclose(CyclWP_summary);
    }
};
