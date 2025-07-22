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
        input_size: Number of input features (2 market features + 4 * lookback_periods price features)
        output_size: Number of outputs (1 for TP multiplier)
        model_path: Path where to save the ONNX model
    """
    
    logging.info(f"Creating model with {input_size} input features")
    
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
    if input_size >= 2:
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

def test_model_simple(model_path: str, test_input: np.ndarray):
    """
    Simple test of the created ONNX model with one input.
    """
    try:
        # Load the model
        session = ort.InferenceSession(model_path)
        
        # Get input and output names
        input_name = session.get_inputs()[0].name
        output_name = session.get_outputs()[0].name

        # Run inference
        result = session.run([output_name], {input_name: test_input})
        tp_multiplier = result[0][0][0]
        
        logging.info(f"Test: Input shape={test_input.shape} → TP Multiplier={tp_multiplier:.3f}")
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

// Choose the model file based on desired lookback periods:
// For lookback_periods = 1: use tp_model_lookback_1.onnx (6 features total: 2 + 1*4)
// For lookback_periods = 2: use tp_model_lookback_2.onnx (10 features total: 2 + 2*4)
// For lookback_periods = 3: use tp_model_lookback_3.onnx (14 features total: 2 + 3*4)
// For lookback_periods = 5: use tp_model_lookback_5.onnx (22 features total: 2 + 5*4)

config.rl_model_path = "/path/to/Models/tp_model_lookback_2.onnx";  // Example for 2 candles
config.rl_lookback_periods = 2;  // Must match the model!
config.rl_tp_min_multiplier = 0.5;   // Minimum TP multiplier
config.rl_tp_max_multiplier = 5.0;   // Maximum TP multiplier

// The model expects (2 + lookback_periods * 4) features in this order:
// 1. current_atr (float)
// 2. stop_loss_distance (float)
// 3+. Normalized OHLC data from recent candles:
//     For each candle: (open/current_price - 1), (high/current_price - 1), 
//                     (low/current_price - 1), (close/current_price - 1)

// IMPORTANT: The lookback_periods value in config must match the model file!
// If you set lookback_periods=3 but use tp_model_lookback_2.onnx, it will fail.
"""
    
    with open("../models/onnx_model_usage_example.txt", "w") as f:
        f.write(config_example)

    logging.info("📝 Created usage example: onnx_model_usage_example.txt")

def main():
    logging.info("🤖 Creating test ONNX models for calculateTakeProfitWithRL function...")

    # Create models for different lookback periods
    lookback_periods = [1, 2, 3, 5]  # Common lookback periods
    
    for lookback in lookback_periods:
        # Model configuration
        input_size = 2 + (lookback * 4)  # 2 market features + lookback * 4 OHLC features
        model_path = f"../models/tp_model_lookback_{lookback}.onnx"
        
        logging.info(f"\n📈 Creating model for lookback_periods={lookback} (input_size={input_size})")

        try:
            # Create the model
            created_model = create_simple_tp_model(input_size, 1, model_path)
            
            # Test the model with appropriate input size
            test_input_size = input_size
            test_input = np.random.randn(1, test_input_size).astype(np.float32) * 0.1
            test_input[0][0] = 0.1  # ATR
            test_input[0][1] = 0.05  # SL distance
            
            if test_model_simple(created_model, test_input):
                logging.info(f"✅ Model for lookback {lookback} created successfully")
            else:
                logging.warning(f"⚠️ Model for lookback {lookback} created but test failed")
                
        except Exception as e:
            logging.error(f"❌ Error creating model for lookback {lookback}: {e}")
            continue
    
    # Create usage example
    create_config_example()

    logging.info(f"\n🎉 Success! Multiple test ONNX models created:")
    for lookback in lookback_periods:
        model_path = f"../models/tp_model_lookback_{lookback}.onnx"
        logging.info(f"   📁 Lookback {lookback}: {model_path}")
    
    logging.info(f"   📋 Usage example: ../models/onnx_model_usage_example.txt")
    logging.info(f"\n💡 To test your C++ function:")
    logging.info(f"   1. Set config.rl_model_path to the appropriate model file")
    logging.info(f"   2. Set config.rl_lookback_periods to match the model (1, 2, 3, or 5)")
    logging.info(f"   3. Call PositionManager::calculateTakeProfit() with use_rl_for_tp = true")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
