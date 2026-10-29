from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
REQUIRED_MESSAGE = "Required result must be produced by an earlier enabled pass"
PRELUDE = r'''
#include "modules/renderer/RenderPipeline.h"

struct GeometryResult {};
struct LightingResult {};
struct UnusedResult {};
struct Depth {};

class Producer final : public RenderPass {
public:
    using Requires = TypeList<>;
    using Optional = TypeList<>;
    using Produces = TypeList<Depth>;
    void execute(RenderContext&, FrameContext&) override {}
};

class GeometryPass final : public RenderPass {
public:
    using Requires = TypeList<>;
    using Optional = TypeList<>;
    using Produces = TypeList<GeometryResult>;
    explicit GeometryPass(int value = 0) : value(value) {}
    void execute(RenderContext&, FrameContext&) override {}
    int value;
};

class LightingPass final : public RenderPass {
public:
    using Requires = TypeList<GeometryResult>;
    using Optional = TypeList<>;
    using Produces = TypeList<LightingResult>;
    void execute(RenderContext&, FrameContext&) override {}
};

class PresentPass final : public RenderPass {
public:
    using Requires = TypeList<LightingResult>;
    using Optional = TypeList<>;
    using Produces = TypeList<>;
    void execute(RenderContext&, FrameContext&) override {}
};

class OptionalPass final : public RenderPass {
public:
    using Requires = TypeList<>;
    using Optional = TypeList<GeometryResult, Depth>;
    using Produces = TypeList<>;
    void execute(RenderContext&, FrameContext&) override {}
};

class UnusedPass final : public RenderPass {
public:
    using Requires = TypeList<>;
    using Optional = TypeList<>;
    using Produces = TypeList<UnusedResult>;
    void execute(RenderContext&, FrameContext&) override {}
};
'''


PIPELINE_FIXTURE = r'''
RenderPipeline::RenderPipeline() {
    passes_ = render_pipeline_detail::Registration<>{}
        .addPass<GeometryPass>(42)
        .addPass<LightingPass>()
        .addPass<Producer>()
        .addPass<UnusedPass>()
        .addPass<OptionalPass>()
        .finish();
}
'''


class RenderPipelineTest(unittest.TestCase):
    def compile_program(
        self, body, expected_failure=False,
        expected_diagnostics=("static assertion failed", REQUIRED_MESSAGE),
        fixture="",
    ):
        with tempfile.TemporaryDirectory(prefix="render_pipeline_test_") as directory:
            source = Path(directory) / "test.cpp"
            executable = Path(directory) / "test"
            source.write_text(PRELUDE + fixture + "\nint main() {\n" + body + "\n}\n")
            result = subprocess.run(
                [
                    "clang++", "-std=c++20", "-Isrc", str(source),
                    "-o", str(executable),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
                timeout=30,
            )
            diagnostics = result.stdout + result.stderr
            if expected_failure:
                self.assertNotEqual(result.returncode, 0, diagnostics)
                for message in expected_diagnostics:
                    self.assertIn(message, diagnostics)
                return
            self.assertEqual(result.returncode, 0, diagnostics)
            executed = subprocess.run(
                [str(executable)],
                cwd=ROOT,
                capture_output=True,
                text=True,
                timeout=5,
            )
            self.assertEqual(
                executed.returncode, 0, executed.stdout + executed.stderr
            )

    def test_forged_result_fails_compilation(self):
        self.compile_program(r'''
    auto registration = render_pipeline_detail::Registration<TypeList<Depth>>{};
''', expected_failure=True, expected_diagnostics=(
            "no matching constructor", "constraints not satisfied",
        ))

    def test_forged_pass_fails_compilation(self):
        self.compile_program(r'''
    auto registration = render_pipeline_detail::Registration<TypeList<>, Producer>{};
''', expected_failure=True, expected_diagnostics=(
            "no matching constructor", "constraints not satisfied",
        ))

    def test_pipeline_mandatory_producer_cannot_disable(self):
        self.compile_program(r'''
    RenderPipeline pipeline;
    auto* producer = pipeline.findPass<GeometryPass>();
    if (!producer || producer->value != 42) return 1;
    if (pipeline.setEnabled<GeometryPass>(false)) return 2;
    if (!pipeline.setEnabled<GeometryPass>(true)) return 3;
    if (pipeline.setEnabled<GeometryPass>(false)) return 4;
    if (pipeline.findPass<GeometryPass>() != producer) return 5;
    return 0;
''', fixture=PIPELINE_FIXTURE)

    def test_pipeline_optional_and_unused_producers_can_toggle(self):
        self.compile_program(r'''
    RenderPipeline pipeline;
    auto* optionalProducer = pipeline.findPass<Producer>();
    auto* unusedProducer = pipeline.findPass<UnusedPass>();
    if (!optionalProducer || !unusedProducer) return 1;
    if (!pipeline.setEnabled<Producer>(false)) return 2;
    if (!pipeline.setEnabled<UnusedPass>(false)) return 3;
    if (pipeline.findPass<Producer>() != optionalProducer) return 4;
    if (pipeline.findPass<UnusedPass>() != unusedProducer) return 5;
    if (!pipeline.setEnabled<Producer>(true)) return 6;
    if (!pipeline.setEnabled<UnusedPass>(true)) return 7;
    if (pipeline.findPass<Producer>() != optionalProducer) return 8;
    if (pipeline.findPass<UnusedPass>() != unusedProducer) return 9;
    return 0;
''', fixture=PIPELINE_FIXTURE)

    def test_pipeline_absent_pass_returns_false_and_null(self):
        self.compile_program(r'''
    RenderPipeline pipeline;
    if (pipeline.findPass<PresentPass>() != nullptr) return 1;
    if (pipeline.setEnabled<PresentPass>(false)) return 2;
    if (pipeline.setEnabled<PresentPass>(true)) return 3;
    return 0;
''', fixture=PIPELINE_FIXTURE)

    def test_registration_valid_order(self):
        self.compile_program(r'''
    auto entries = render_pipeline_detail::Registration<>{}
        .addPass<GeometryPass>(42)
        .addPass<LightingPass>()
        .addPass<PresentPass>()
        .finish();
    if (entries.size() != 3) return 1;
    auto* geometry = dynamic_cast<GeometryPass*>(entries[0].pass.get());
    if (!geometry || geometry->value != 42) return 2;
    if (!dynamic_cast<LightingPass*>(entries[1].pass.get())) return 3;
    if (!dynamic_cast<PresentPass*>(entries[2].pass.get())) return 4;
    return 0;
''')

    def test_missing_producer_fails_compilation(self):
        self.compile_program(r'''
    auto entries = render_pipeline_detail::Registration<>{}
        .addPass<LightingPass>()
        .finish();
''', expected_failure=True)

    def test_reversed_order_fails_compilation(self):
        self.compile_program(r'''
    auto entries = render_pipeline_detail::Registration<>{}
        .addPass<PresentPass>()
        .addPass<LightingPass>()
        .addPass<GeometryPass>()
        .finish();
''', expected_failure=True)

    def test_optional_missing_succeeds(self):
        self.compile_program(r'''
    auto entries = render_pipeline_detail::Registration<>{}
        .addPass<OptionalPass>()
        .finish();
    if (entries.size() != 1) return 1;
    if (!dynamic_cast<OptionalPass*>(entries[0].pass.get())) return 2;
    if (!entries[0].enabled || entries[0].required) return 3;
    return 0;
''')

    def test_removing_producer_fails_compilation(self):
        self.compile_program(r'''
    auto entries = render_pipeline_detail::Registration<>{}
        .addPass<GeometryPass>()
        .addPass<PresentPass>()
        .finish();
''', expected_failure=True)

    def test_entry_required_and_enabled_flags(self):
        self.compile_program(r'''
    auto entries = render_pipeline_detail::Registration<>{}
        .addPass<GeometryPass>()
        .addPass<LightingPass>()
        .addPass<PresentPass>()
        .addPass<UnusedPass>()
        .addPass<OptionalPass>()
        .finish();
    if (entries.size() != 5) return 1;
    if (!entries[0].required || !entries[1].required) return 2;
    if (entries[2].required || entries[3].required || entries[4].required) return 3;
    for (const auto& entry : entries) {
        if (!entry.pass || !entry.enabled) return 4;
    }
    auto optionalEntries = render_pipeline_detail::Registration<>{}
        .addPass<GeometryPass>()
        .addPass<OptionalPass>()
        .finish();
    if (optionalEntries[0].required || optionalEntries[1].required) return 5;
    return 0;
''')


if __name__ == "__main__":
    unittest.main()
