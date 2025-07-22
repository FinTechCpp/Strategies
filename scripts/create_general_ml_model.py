#!/usr/bin/env python3
"""
Script to create a general ML model for take profit prediction.

This demonstrates the new general interface where the model:
1. Receives raw market features (price, ATR, SL distance, historical candles)
2. Outputs take profit distance directly (not a multiplier)
3. Handles feature engineering internally

The model is responsible for:
- Feature normalization/scaling
- Determining which features to use
- Outputting meaningful take profit distances
"""

import numpy as np
import onnx
from onnx import helper, TensorProto
import onnxruntime as ort
import logging

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

def create_general_tp_model(lookback_periods: int = 3, model_path: str = "general_tp_model.onnx"):
    """
    Create a general ML model that predicts take profit distance directly.
    
    Args:
        lookback_periods: Number of historical candles to include in features
        model_path: Path where to save the ONNX model
    
    Features input format:
        - Index 0: current_price
        - Index 1: current_atr  
        - Index 2: stop_loss_distance
        - Index 3+: Historical OHLC data (4 values per candle: O, H, L, C)
    
    Output:
        - Single value: predicted take profit distance
    """
    
    # Calculate input size: 3 basic features + (lookback_periods * 4 OHLC values)
    input_size = 3 + (lookback_periods * 4)
    output_size = 1
    
    logging.info(f"Creating general ML model with {input_size} input features")
    logging.info(f"Features: price, ATR, SL_distance + {lookback_periods} candles (OHLC)")
    
    # Define the input and output tensors
    input_tensor = helper.make_tensor_value_info(
        'input', TensorProto.FLOAT, [1, input_size]
    )
    
    output_tensor = helper.make_tensor_value_info(
        'output', TensorProto.FLOAT, [1, output_size]
    )
    
    # Create a more sophisticated model with multiple layers
    np.random.seed(42)  # For reproducible results
    
    # Layer 1: Input to hidden (with feature scaling built-in)
    hidden_size = 16
    W1 = np.random.randn(input_size, hidden_size).astype(np.float32) * 0.1
    b1 = np.zeros(hidden_size, dtype=np.float32)
    
    # Add some domain knowledge to the weights
    if input_size >= 3:
        # Give more importance to ATR and SL distance
        W1[1, :4] = 0.5   # ATR weights
        W1[2, 4:8] = 0.3  # SL distance weights
    
    # Layer 2: Hidden to output
    W2 = np.random.randn(hidden_size, output_size).astype(np.float32) * 0.1
    b2 = np.array([0.02], dtype=np.float32)  # Start with small positive TP distance
    
    # Create weight and bias tensors
    W1_tensor = helper.make_tensor('W1', TensorProto.FLOAT, W1.shape, W1.flatten())
    b1_tensor = helper.make_tensor('b1', TensorProto.FLOAT, b1.shape, b1.flatten())
    W2_tensor = helper.make_tensor('W2', TensorProto.FLOAT, W2.shape, W2.flatten())
    b2_tensor = helper.make_tensor('b2', TensorProto.FLOAT, b2.shape, b2.flatten())
    
    # Create network nodes
    # Layer 1: input * W1 + b1
    matmul1_node = helper.make_node(
        'MatMul',
        inputs=['input', 'W1'],
        outputs=['hidden_linear']
    )
    
    add1_node = helper.make_node(
        'Add',
        inputs=['hidden_linear', 'b1'],
        outputs=['hidden_with_bias']
    )
    
    # ReLU activation
    relu_node = helper.make_node(
        'Relu',
        inputs=['hidden_with_bias'],
        outputs=['hidden_activated']
    )
    
    # Layer 2: hidden * W2 + b2
    matmul2_node = helper.make_node(
        'MatMul',
        inputs=['hidden_activated', 'W2'],
        outputs=['output_linear']
    )
    
    add2_node = helper.make_node(
        'Add',
        inputs=['output_linear', 'b2'],
        outputs=['output_with_bias']
    )
    
    # Apply positive constraint (ensure TP distance is always positive)
    relu_output_node = helper.make_node(
        'Relu',
        inputs=['output_with_bias'],
        outputs=['output']
    )
    
    # Create the graph
    graph = helper.make_graph(
        nodes=[matmul1_node, add1_node, relu_node, matmul2_node, add2_node, relu_output_node],
        name='GeneralTakeProfitModel',
        inputs=[input_tensor],
        outputs=[output_tensor],
        initializer=[W1_tensor, b1_tensor, W2_tensor, b2_tensor]
    )
    
    # Create the model
    model = helper.make_model(graph, producer_name='TradingBot')
    model.opset_import[0].version = 11
    model.ir_version = 8
    
    # Check and save the model
    onnx.checker.check_model(model)
    onnx.save(model, model_path)

    logging.info(f"✅ General ML model created successfully: {model_path}")
    return model_path

def test_general_model(model_path: str, lookback_periods: int = 3):
    """
    Test the general ML model with realistic market data.
    """
    try:
        # Load the model
        session = ort.InferenceSession(model_path)
        
        # Get input and output info
        input_name = session.get_inputs()[0].name
        output_name = session.get_outputs()[0].name
        input_shape = session.get_inputs()[0].shape

        logging.info(f"📊 Model input: {input_name}, shape: {input_shape}")
        logging.info(f"📊 Model output: {output_name}")

        # Create realistic test cases
        test_cases = [
            {
                "name": "High volatility EUR/USD",
                "price": 1.0850,
                "atr": 0.0012,
                "sl_distance": 0.0008,
                "candles": [(1.0845, 1.0860, 1.0840, 1.0855), (1.0855, 1.0865, 1.0850, 1.0858), (1.0858, 1.0865, 1.0852, 1.0850)]
            },
            {
                "name": "Low volatility AAPL",
                "price": 150.25,
                "atr": 0.85,
                "sl_distance": 1.20,
                "candles": [(149.80, 150.50, 149.70, 150.10), (150.10, 150.40, 149.95, 150.25), (150.25, 150.35, 150.00, 150.20)]
            },
            {
                "name": "Trending NDX",
                "price": 15750.0,
                "atr": 45.0,
                "sl_distance": 65.0,
                "candles": [(15700, 15780, 15690, 15750), (15750, 15800, 15740, 15785), (15785, 15820, 15770, 15750)]
            }
        ]

        logging.info(f"\n🧪 Testing general model with {lookback_periods} lookback periods:")
        
        for test_case in test_cases:
            # Prepare input features
            features = [
                test_case["price"],
                test_case["atr"], 
                test_case["sl_distance"]
            ]
            
            # Add candle data
            for i in range(lookback_periods):
                if i < len(test_case["candles"]):
                    candle = test_case["candles"][i]
                    features.extend(candle)  # Add O, H, L, C
                else:
                    features.extend([0.0, 0.0, 0.0, 0.0])  # Padding with zeros
            
            # Convert to numpy array with proper shape
            test_input = np.array([features], dtype=np.float32)
            
            # Run inference
            result = session.run([output_name], {input_name: test_input})
            predicted_tp = result[0][0][0]
            
            # Calculate some metrics for comparison
            tp_to_sl_ratio = predicted_tp / test_case["sl_distance"]
            tp_percentage = (predicted_tp / test_case["price"]) * 100
            
            logging.info(f"📈 {test_case['name']}:")
            logging.info(f"   Input: Price={test_case['price']}, ATR={test_case['atr']}, SL={test_case['sl_distance']}")
            logging.info(f"   Predicted TP: {predicted_tp:.4f}")
            logging.info(f"   TP/SL Ratio: {tp_to_sl_ratio:.2f}")
            logging.info(f"   TP %: {tp_percentage:.3f}%")
            logging.info("")

        return True
        
    except Exception as e:
        logging.error(f"❌ Error testing model: {e}")
        return False
    
    with open("../models/general_ml_interface_documentation.md", "w") as f:
        f.write(doc_content)
    
    logging.info("📝 Created documentation: general_ml_interface_documentation.md")

def main():
    logging.info("🤖 Creating general ML model for take profit prediction...")

    # Create models for different lookback periods
    lookback_periods = [50, 80, 100, 120, 150]  # 0 = no historical data, others with history

    for lookback in lookback_periods:
        model_name = f"general_tp_model_lookback_{lookback}.onnx"
        model_path = f"../models/{model_name}"
        
        logging.info(f"\n📈 Creating general model for {lookback} lookback periods")

        try:
            # Create the model
            create_general_tp_model(lookback, model_path)
            
            # Test the model
            test_general_model(model_path, lookback)
                
        except Exception as e:
            logging.error(f"❌ Error creating model for lookback {lookback}: {e}")
    
    logging.info("\n✅ General ML interface setup complete!")
    logging.info("🔬 Train your own models using the documented feature format")

if __name__ == "__main__":
    main()
