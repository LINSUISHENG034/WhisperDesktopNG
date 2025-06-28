  Step 1: 在 `GGML/whisper.h` 中添加函数声明


   * 位置: 建议将其放置在 whisper_get_logits 或其他 whisper_get_*
     函数附近，以保持API的逻辑分组和一致性。例如，可以放在 WHISPER_API float * whisper_get_logits(struct
     whisper_context * ctx); 之后。
   * 文档注释: 强烈建议添加详细的文档注释。
     这对于API的清晰度和未来的可维护性至关重要。注释应说明其用途、返回类型、参数以及任何使用注意事项。


  `c++
      // Returns the underlying ggml_context for advanced tensor manipulation.
      // This function provides direct access to the GGML context, allowing iteration
      // and inspection of model tensors. Use with caution, as direct GGML operations
      // might require a deeper understanding of the GGML library.
      WHISPER_API struct ggml_context  whisper_get_ggml_context(struct whisper_context  ctx);

   1
   2 *   **函数命名:** `whisper_get_ggml_context` 命名非常合适，它清晰地表达了函数的功能，并且与
     `whisper.cpp` 现有的 `whisper_get_*` 命名约定保持一致。
   3
   4 **Step 2: 在 `GGML/whisper.cpp` 中实现函数**
   5

   * 对 `whisper_context` 内部结构的假设是否正确？`return ctx->model.ctx;`
      这是最关键的一点。我无法直接访问 whisper.cpp 的内部实现来验证 ctx->model.ctx
  是否是正确的访问路径。您需要手动检查 `GGML/whisper.cpp` 的源代码，确认 whisper_context 结构体中确实有一个
  model 成员，并且 model 成员中包含一个 ggml_context* 类型的 ctx
  成员。如果路径不同，请根据实际代码进行调整。

      在您确认之前，我无法为您生成此部分的修改代码。


   * 是否需要添加额外的错误检查？
      是的，您已经考虑到了 if (!ctx) 的 nullptr 检查，这很好。这是基本的安全措施。


   * 返回的 `ggml_context` 是否是我们需要用于张量迭代的正确 context？
      是的，ggml_context 是 ggml_get_first_tensor() 和 ggml_get_next_tensor()
  函数操作的对象。一旦您确认了正确的访问路径，返回的 ggml_context* 将是用于张量迭代的正确上下文。

  Step 3: 更新 `QuantizationReferenceChecker::findTensor`


   * 这个迭代逻辑是否正确？
      是的，您之前提供的迭代逻辑是正确的。它将遍历 ggml_context 中的所有张量，并根据名称进行匹配。


   * 是否需要添加调试信息来显示所有可用的张量名称？
      强烈建议在开发和调试阶段添加。 这对于理解模型内部的张量结构、确认您要查找的张量名称以及调试任何潜在问
  题都非常有帮助。在生产代码中，您可以选择性地移除或通过编译宏控制这些调试信息。


  `c++
      // 示例调试代码
      // ggml_tensor* current_tensor = ggml_get_first_tensor(ctx);
      // while (current_tensor != nullptr) {
      //     printf("Found tensor: %s\n", current_tensor->name);
      //     if (strcmp(current_tensor->name, name) == 0) {
      //         return current_tensor;
      //     }
      //     current_tensor = ggml_get_next_tensor(ctx, current_tensor);
      // }

   1

   * 性能考虑：对于大模型，这种线性搜索是否可接受？
      对于 findTensor 这种通常在模型加载或初始化阶段调用一次以查找特定张量的函数，线性搜索的性能通常是可接受
  的。模型的张量数量虽然多，但通常不会达到需要哈希表级别优化的程度。


      如果 findTensor 在性能敏感的循环中被频繁调用，那么线性搜索可能会成为瓶颈。在这种情况下，您可以在第一次
  找到所有需要的张量后，将它们的指针存储在一个 std::map<std::string, ggml_tensor*> 中，以便后续进行 O(1)
  的查找。但根据您描述的用途，这可能不是必需的优化。

  Step 4: 测试和验证

   * 您的测试计划非常完善，涵盖了编译、功能和边界测试。请务必严格执行这些测试，以确保新API的稳定性和正确性。

  总结和下一步

  您的实施规划非常扎实。最关键的下一步是：


   1. 手动检查 `GGML/whisper.cpp` 的源代码，以确认 whisper_context 内部访问 ggml_context
      的正确路径（例如，是否真的是 ctx->model.ctx）。
   2. 一旦确认了正确的路径，请告诉我，我将为您生成修改 GGML/whisper.h 和 GGML/whisper.cpp 的代码。


  请告诉我您检查 GGML/whisper.cpp 后的结果。