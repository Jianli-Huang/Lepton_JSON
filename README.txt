项目是一个轻量级（lepton）的JSON
1.符合标准的JSON解析器和生成器
2.通过递归下降进行解析
3.使用标准C语言（C89）
4.仅支持UTF-8 JSON文本
5.仅支持以double存储JSON number类型

提供测试开发
1.包含多种解析正确类型的测试
2.包含多种解析失败的测试
3.包含往返（roundtrip）测试，即JSON文本解析和JSON文本生成