#!/usr/bin/env python3
"""
Script to create a simple ONNX model for testing the calculateTakeProfitWithRL function.

This creates a basic neural network that takes market features as input and outputs
a take-profit multiplier.
"""

import numpy as np
import onnx
from onnx import helper, TensorProto
import onnxruntime as ort
import sys
import logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

def create_simple_tp_model(input_size: int = 10, output_size: int = 1, model_path: str = "tp_model.onnx"):
    """
    Create a simple linear model for take-profit prediction.
    
    Args:
        input_size: Number of input features (ATR + SL + price features)
        output_size: Number of outputs (1 for TP multiplier)
        model_path: Path where to save the ONNX model
    """
    
    # Define the input
    input_tensor = helper.make_tensor_value_info(
        'input', TensorProto.FLOAT, [1, input_size]
    )
    
    # Define the output
    output_tensor = helper.make_tensor_value_info(
        'output', TensorProto.FLOAT, [1, output_size]
    )
    
    # Create a simple linear model: output = input * weights + bias
    # Initialize weights with small random values
    np.random.seed(42)  # For reproducible results
    
    # Weights: input_size x output_size
    weights = np.random.randn(input_size, output_size).astype(np.float32) * 0.1
    # Add some logic: give more weight to ATR and SL distance
    weights[0] = 1.5  # ATR weight
    weights[1] = 0.8  # SL distance weight
    
    # Bias to get reasonable TP multiplier around 2.0
    bias = np.array([2.0], dtype=np.float32)
    
    # Create weight tensors
    W_tensor = helper.make_tensor('W', TensorProto.FLOAT, weights.shape, weights.flatten())
    b_tensor = helper.make_tensor('b', TensorProto.FLOAT, bias.shape, bias.flatten())
    
    # Create nodes for: output = max(0.5, input * W + b)
    matmul_node = helper.make_node(
        'MatMul',
        inputs=['input', 'W'],
        outputs=['matmul_output']
    )
    
    add_node = helper.make_node(
        'Add',
        inputs=['matmul_output', 'b'],
        outputs=['output']
    )
    
    # Create the graph (simple linear model)
    graph = helper.make_graph(
        nodes=[matmul_node, add_node],
        name='TakeProfitModel',
        inputs=[input_tensor],
        outputs=[output_tensor],
        initializer=[W_tensor, b_tensor]
    )
    
    # Create the model with older IR version for compatibility
    model = helper.make_model(graph, producer_name='TradingBot')
    model.opset_import[0].version = 11  # Use opset version 11
    model.ir_version = 8  # Set IR version to 8 for better compatibility
    
    # Check the model
    onnx.checker.check_model(model)
    
    # Save the model
    onnx.save(model, model_path)

    logging.info(f"✅ ONNX model created successfully: {model_path}")
    return model_path

def test_model(model_path: str, input_size: int = 10):
    """
    Test the created ONNX model with sample inputs.
    """
    try:
        # Load the model
        session = ort.InferenceSession(model_path)
        
        # Get input and output names
        input_name = session.get_inputs()[0].name
        output_name = session.get_outputs()[0].name

        logging.info(f"📊 Model input: {input_name}, shape: {session.get_inputs()[0].shape}")
        logging.info(f"📊 Model output: {output_name}, shape: {session.get_outputs()[0].shape}")

        # Create test inputs (simulating market features)
        test_cases = [
            # [ATR, SL_distance, price_features...]
            np.array([[0.1, 0.05, 0.01, -0.02, 0.015, -0.01, 0.02, -0.005, 0.01, 0.005]], dtype=np.float32),
            np.array([[0.2, 0.1, -0.01, 0.02, -0.015, 0.01, -0.02, 0.005, -0.01, -0.005]], dtype=np.float32),
            np.array([[0.05, 0.02, 0.005, 0.01, 0.008, -0.003, 0.012, 0.007, -0.002, 0.004]], dtype=np.float32),
        ]

        logging.info("\n🧪 Testing model with sample inputs:")
        for i, test_input in enumerate(test_cases):
            result = session.run([output_name], {input_name: test_input})
            tp_multiplier = result[0][0][0]
            logging.info(f"Test {i+1}: ATR={test_input[0][0]:.3f}, SL={test_input[0][1]:.3f} → TP Multiplier={tp_multiplier:.3f}")

        logging.info("✅ Model test completed successfully!")
        return True
        
    except Exception as e:
        logging.error(f"❌ Error testing model: {e}")
        return False

def create_config_example():
    """
    Create an example showing how to configure the RL model in your C++ code.
    """
    config_example = """
// Example C++ configuration for using the RL model:

StrategyBaseConfig config;
// ... other config settings ...

// Enable RL-based take profit
config.use_rl_for_tp = true;
config.rl_model_path = "/path/to/tp_model.onnx";
config.rl_lookback_periods = 2;  // Number of recent candles to use (2 candles * 4 OHLC = 8 features + 2 market features = 10 total)
config.rl_tp_min_multiplier = 0.5;   // Minimum TP multiplier
config.rl_tp_max_multiplier = 5.0;   // Maximum TP multiplier

// The model expects 10 features in this order:
// 1. current_atr (float)
// 2. stop_loss_distance (float)
// 3-10. Normalized OHLC data from recent candles:
//       For each candle: (open/current_price - 1), (high/current_price - 1), 
//                       (low/current_price - 1), (close/current_price - 1)
"""
    
    with open("../models/onnx_model_usage_example.txt", "w") as f:
        f.write(config_example)

    logging.info("📝 Created usage example: onnx_model_usage_example.txt")

def main():
    logging.info("🤖 Creating test ONNX model for calculateTakeProfitWithRL function...")

    # Model configuration
    input_size = 10  # 2 market features + 8 price features (2 candles * 4 OHLC)
    model_path = "../models/tp_model.onnx"

    try:
        # Create the model
        created_model = create_simple_tp_model(input_size, 1, model_path)
        
        # Test the model
        if test_model(created_model, input_size):
            # Create usage example
            create_config_example()

            logging.info(f"\n🎉 Success! Your test ONNX model is ready:")
            logging.info(f"   📁 Model file: {model_path}")
            logging.info(f"   📋 Usage example: ../models/onnx_model_usage_example.txt")
            logging.info(f"\n💡 To test your C++ function:")
            logging.info(f"   1. Set config.rl_model_path = \"{model_path}\"")
            logging.info(f"   2. Set config.rl_lookback_periods = 2")
            logging.info(f"   3. Call PositionManager::calculateTakeProfit() with use_rl_for_tp = true")
            
        else:
            logging.error("❌ Model creation failed during testing")
            return 1
            
    except Exception as e:
        logging.error(f"❌ Error creating model: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
