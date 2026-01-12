#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/scene_document.hpp"
#include <filesystem>
#include <fstream>

namespace NovelMind::editor {

// ============================================================================
// Templates
// ============================================================================

std::vector<std::string> ProjectManager::getAvailableTemplates() {
  // Returns all available project templates
  // Templates are located in editor/templates/ directory
  return {
      "empty",            // Minimal starting point
      "kinetic_novel",    // Linear story without choices
      "branching_story",  // Interactive story with multiple endings
      "mobile_optimized", // Optimized for mobile devices (portrait)
      "tutorial_project"  // Interactive learning tutorial
  };
}

Result<void> ProjectManager::createProjectFromTemplate(const std::string& templateName) {
  // Template would be copied from editor/templates/<templateName>/
  // For now, just create a basic main.nms script

  namespace fs = std::filesystem;

  fs::path scriptsDir = fs::path(m_projectPath) / "Scripts";
  fs::path mainScript = scriptsDir / "main.nms";
  fs::path scenesDir = fs::path(m_projectPath) / "Scenes";
  fs::create_directories(scenesDir);

  std::ofstream file(mainScript);
  if (!file.is_open()) {
    return Result<void>::error("Failed to create main script");
  }

  if (templateName == "kinetic_novel") {
    file << "// NovelMind Script - Visual Novel (Linear) Template\n";
    file << "// Add images to Assets/Images and update paths below.\n\n";
    file << "character Hero(name=\"Alex\", color=\"#ffcc00\")\n";
    file << "character Narrator(name=\"Narrator\", color=\"#cccccc\")\n\n";
    file << "scene main {\n";
    file << "    show background \"title.png\"\n";
    file << "    say \"Welcome to your visual novel!\"\n";
    file << "    say \"Replace this script with your story.\"\n";
    file << "    Hero \"Let's begin.\"\n";
    file << "}\n";
    m_metadata.startScene = "main";
  } else if (templateName == "branching_story") {
    file << "// NovelMind Script - Branching Story Template\n";
    file << "// Add images to Assets/Images and update paths below.\n\n";
    file << "character Hero(name=\"Alex\", color=\"#ffcc00\")\n\n";
    file << "scene main {\n";
    file << "    show background \"crossroads.png\"\n";
    file << "    say \"Welcome to your interactive story!\"\n";
    file << "    Hero \"Which path should we take?\"\n";
    file << "    choice {\n";
    file << "        \"Go left\" -> goto left_path\n";
    file << "        \"Go right\" -> goto right_path\n";
    file << "    }\n";
    file << "}\n\n";
    file << "scene left_path {\n";
    file << "    show background \"forest_path.png\"\n";
    file << "    say \"You chose the left path.\"\n";
    file << "    goto ending\n";
    file << "}\n\n";
    file << "scene right_path {\n";
    file << "    show background \"city_path.png\"\n";
    file << "    say \"You chose the right path.\"\n";
    file << "    goto ending\n";
    file << "}\n\n";
    file << "scene ending {\n";
    file << "    say \"This is the end of the demo. Expand it with your own "
            "scenes!\"\n";
    file << "}\n";
    m_metadata.startScene = "main";
  } else if (templateName == "mobile_optimized") {
    file << "// NovelMind Script - Mobile Optimized Template\n";
    file << "// Portrait orientation (1080x1920) for mobile devices\n\n";
    file << "character Hero(name=\"Hero\", color=\"#4A90D9\")\n";
    file << "character Narrator(name=\"\", color=\"#AAAAAA\")\n\n";
    file << "scene main {\n";
    file << "    show background \"mobile_bg.png\"\n";
    file << "    transition fade 1.0\n";
    file << "    say Narrator \"Welcome to your mobile visual novel!\"\n";
    file << "    say Narrator \"This template is optimized for mobile "
            "devices.\"\n";
    file << "    show Hero at center\n";
    file << "    Hero \"Let's create something amazing!\"\n";
    file << "}\n";
    m_metadata.startScene = "main";
    m_metadata.targetResolution = "1080x1920";
  } else if (templateName == "tutorial_project") {
    file << "// NovelMind Script - Interactive Tutorial\n";
    file << "// Learn NovelMind step-by-step!\n\n";
    file << "character Teacher(name=\"Prof. Tutorial\", color=\"#4A90D9\")\n";
    file << "character Narrator(name=\"\", color=\"#AAAAAA\")\n\n";
    file << "scene main {\n";
    file << "    show background \"tutorial_bg.png\"\n";
    file << "    transition fade 1.0\n";
    file << "    say Narrator \"Welcome to the NovelMind Tutorial!\"\n";
    file << "    show Teacher at center\n";
    file << "    Teacher \"I'll teach you how to create visual novels.\"\n";
    file << "    Teacher \"Check the README.md for detailed lessons.\"\n";
    file << "}\n";
    m_metadata.startScene = "main";
  } else {
    file << "// NovelMind Script\n\n";
    file << "scene main {\n";
    file << "    say \"Hello, World!\"\n";
    file << "}\n";
    m_metadata.startScene = "main";
  }

  file.close();

  auto createSceneDoc = [&](const std::string& sceneId, const std::string& bgAsset,
                            bool includeHero) -> void {
    SceneDocument doc;
    doc.sceneId = sceneId;
    int z = 0;
    if (!bgAsset.empty()) {
      SceneDocumentObject bg;
      bg.id = "background_" + sceneId;
      bg.name = "Background";
      bg.type = "Background";
      bg.zOrder = z++;
      bg.properties["textureId"] = bgAsset;
      doc.objects.push_back(std::move(bg));
    }
    if (includeHero) {
      SceneDocumentObject hero;
      hero.id = "character_hero_" + sceneId;
      hero.name = "Hero";
      hero.type = "Character";
      hero.zOrder = z++;
      hero.x = 0.0f;
      hero.y = 0.0f;
      hero.properties["characterId"] = "Hero";
      hero.properties["textureId"] = "hero.png";
      doc.objects.push_back(std::move(hero));
    }

    const fs::path scenePath = scenesDir / (sceneId + ".nmscene");
    saveSceneDocument(doc, scenePath.string());
  };

  if (templateName == "branching_story") {
    createSceneDoc("main", "crossroads.png", true);
    createSceneDoc("left_path", "forest_path.png", true);
    createSceneDoc("right_path", "city_path.png", true);
    createSceneDoc("ending", "", false);
  } else if (templateName == "kinetic_novel") {
    createSceneDoc("main", "title.png", true);
  } else if (templateName == "mobile_optimized") {
    createSceneDoc("main", "mobile_bg.png", true);
  } else if (templateName == "tutorial_project") {
    createSceneDoc("main", "tutorial_bg.png", true);
  } else {
    createSceneDoc("main", "", false);
  }

  return Result<void>::ok();
}

} // namespace NovelMind::editor
