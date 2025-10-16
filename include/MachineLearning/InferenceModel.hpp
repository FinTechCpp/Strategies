#pragma once
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <onnxruntime_cxx_api.h>  // Need to include ONNX Runtime

class InferenceModel {
private:
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;
    
    // Store names as strings to avoid lifetime issues
    std::vector<std::string> input_names_storage;
    std::vector<std::string> output_names_storage;
    
    // Keep const char* pointers for ONNX API
    std::vector<const char*> input_names;
    std::vector<const char*> output_names;
    
    bool is_initialized = false;
    
public:
    InferenceModel() : env(ORT_LOGGING_LEVEL_WARNING, "InferenceModel") {}
    
    bool load(const std::string& model_path) {
        try {
            Ort::SessionOptions session_options;
#ifdef _WIN32
            // Convert string to wide string for Windows
            std::wstring wide_path(model_path.begin(), model_path.end());
            session = std::make_unique<Ort::Session>(env, wide_path.c_str(), session_options);
#else
            session = std::make_unique<Ort::Session>(env, model_path.c_str(), session_options);
#endif
            
            // Get input and output names
            Ort::AllocatorWithDefaultOptions allocator;
            size_t num_input_nodes = session->GetInputCount();
            size_t num_output_nodes = session->GetOutputCount();
            
            input_names_storage.clear();
            output_names_storage.clear();
            input_names.clear();
            output_names.clear();
            
            // Store names as std::string first
            for (size_t i = 0; i < num_input_nodes; i++) {
                auto name = session->GetInputNameAllocated(i, allocator);
                input_names_storage.push_back(std::string(name.get()));
            }
            
            for (size_t i = 0; i < num_output_nodes; i++) {
                auto name = session->GetOutputNameAllocated(i, allocator);
                output_names_storage.push_back(std::string(name.get()));
            }
            
            // Now create const char* pointers from the stored strings
            for (const auto& name : input_names_storage) 
                input_names.push_back(name.c_str());
            
            for (const auto& name : output_names_storage) 
                output_names.push_back(name.c_str());
            
            is_initialized = true;
            return true;
        } catch (const Ort::Exception&) {
            is_initialized = false;
            return false;
        }
    }
    
    bool initialized() const {
        return is_initialized;
    }
    
    float predict(const std::vector<float>& features) {
        if (!is_initialized) throw std::runtime_error("Model not initialized");

        // Create input tensor
        std::vector<int64_t> input_shape = {1, static_cast<int64_t>(features.size())};
        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, 
            const_cast<float*>(features.data()), 
            features.size(), 
            input_shape.data(), 
            input_shape.size()
        );
        
        // Run inference
        std::vector<Ort::Value> output_tensors = session->Run(
            Ort::RunOptions{nullptr}, 
            input_names.data(), 
            &input_tensor, 
            1, 
            output_names.data(), 
            output_names.size()
        );
        
        // Get output value
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        return output_data[0];  // Return first output value
    }
};