#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    constexpr int kHeaderHeight = 54;
    constexpr int kTabsY = 422;
    constexpr int kPanelsY = 452;
    constexpr int kStatusY = 632;

    constexpr float kMinScale = 0.5f;
    constexpr float kMaxScale = 2.0f;

    const juce::String kDefaultStatus = "Hover a control for help - double-click to reset, shift-drag for fine tuning";
}

//==============================================================================
MainView::MainView (RecklessChorusProcessor& p, RecklessLookAndFeel& l)
    : processor (p),
      lnf (l),
      snapshot (p.getState()),
      mix (p.getState(), ParamIDs::mix, "MIX", true),
      output (p.getState(), ParamIDs::output),
      engine (p.getState()),
      speed (p.getState(), ParamIDs::speed, "SPEED"),
      depth (p.getState(), ParamIDs::depth, "DEPTH"),
      quality (p.getState(), ParamIDs::quality, "QUALITY"),
      edge (p.getState(), ParamIDs::edge, "EDGE"),
      voices (p.getState(), ParamIDs::voices, "VOICES"),
      shape (p.getState(), ParamIDs::shape, IconToggle::Kind::Shape),
      wide (p.getState(), ParamIDs::wide, IconToggle::Kind::Wide),
      inputPanel (p.getState()),
      aliasingPanel (p.getState()),
      stabilityPanel (p.getState()),
      companderPanel (p.getState())
{
    setOpaque (true);

    for (auto* c : std::initializer_list<juce::Component*> {
             &prevPreset, &nextPreset, &presetName, &abButton, &mix, &output, &visualizer, &engine,
             &speed, &depth, &quality, &edge, &voices, &inputTab, &analogTab, &shape, &wide,
             &inputPanel, &aliasingPanel, &stabilityPanel, &companderPanel, &sizeButton })
        addAndMakeVisible (c);

    // Presets
    const auto step = [this] (int delta)
    {
        const auto n = processor.getNumPrograms();
        processor.setCurrentProgram ((processor.getCurrentProgram() + delta + n) % n);
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (1); };
    presetName.onClick = [this] { showPresetMenu(); };
    presetName.setComponentID ("preset");

    abButton.boxed = true;
    abButton.setComponentID ("ab");
    abButton.onClick = [this] { processor.toggleAB(); };

    inputTab.onClick = [this] { selectTab (false); };
    analogTab.onClick = [this] { selectTab (true); };
    selectTab (true);

    sizeButton.boxed = true;
    sizeButton.setComponentID ("size");
    sizeButton.onClick = [this] { showSizeMenu(); };

    // Receive mouse enter/exit from every child for the status bar.
    addMouseListener (this, true);
    statusText = kDefaultStatus;
}

MainView::~MainView()
{
    removeMouseListener (this);
}

void MainView::selectTab (bool analogSettings)
{
    showingAnalogSettings = analogSettings;
    analogTab.setSelected (analogSettings);
    inputTab.setSelected (! analogSettings);

    inputPanel.setVisible (! analogSettings);
    for (auto* c : std::initializer_list<juce::Component*> { &aliasingPanel, &stabilityPanel, &companderPanel })
        c->setVisible (analogSettings);
}

//==============================================================================
void MainView::paint (juce::Graphics& g)
{
    const auto accent = lnf.getAccent();

    g.fillAll (Palette::background);

    // Header
    auto header = getLocalBounds().removeFromTop (kHeaderHeight).toFloat();
    g.setColour (Palette::header);
    g.fillRect (header);
    g.setColour (Palette::knobRim);
    g.drawHorizontalLine (kHeaderHeight - 1, 0.0f, (float) getWidth());

    {
        auto titleArea = header.withTrimmedLeft (18.0f).withWidth (230.0f);
        juce::GlyphArrangement ga;
        ga.addLineOfText (Fonts::bold (22.0f), "RECKLESS", 0.0f, 0.0f);
        const auto reckWidth = ga.getBoundingBox (0, -1, true).getWidth();

        g.setColour (Palette::text);
        g.setFont (Fonts::bold (22.0f));
        g.drawText ("RECKLESS", titleArea, juce::Justification::centredLeft);

        g.setColour (accent);
        g.setFont (Fonts::regular (22.0f));
        g.drawText ("CHORUS", titleArea.withTrimmedLeft (reckWidth + 6.0f), juce::Justification::centredLeft);
    }

    // Separators: tabs row and panels
    g.setColour (Palette::knobRim);
    g.fillRect (0, kPanelsY - 1, getWidth(), 1);
    g.fillRect (0, kStatusY - 1, getWidth(), 1);

    // Status bar
    auto status = juce::Rectangle<int> (0, kStatusY, getWidth(), getHeight() - kStatusY).toFloat();
    g.setColour (Palette::header);
    g.fillRect (status);
    status = status.withTrimmedLeft (16.0f).withTrimmedRight (30.0f); // keep clear of the resize corner

    g.setColour (Palette::textDim);
    g.setFont (Fonts::regular (11.5f));
    g.drawText ("v" JucePlugin_VersionString, status, juce::Justification::centredRight);

    status.removeFromRight (122.0f);
    if (statusName.isNotEmpty())
    {
        g.setColour (Palette::text);
        g.setFont (Fonts::bold (12.5f));
        g.drawText (statusName, status, juce::Justification::centredLeft);
        status.removeFromLeft (juce::GlyphArrangement::getStringWidth (Fonts::bold (12.5f), statusName) + 10.0f);
    }
    g.setColour (Palette::text.withAlpha (0.8f));
    g.setFont (Fonts::regular (12.5f));
    g.drawText (statusText, status, juce::Justification::centredLeft, true);
}

void MainView::resized()
{
    // Header
    prevPreset.setBounds (262, 17, 18, 20);
    nextPreset.setBounds (282, 17, 18, 20);
    presetName.setBounds (310, 12, 180, 30);
    abButton.setBounds (506, 15, 30, 24);
    mix.setBounds (556, 4, 46, 46);
    output.setBounds (618, 15, 124, 24);

    // Main section
    engine.setBounds (250, 62, 260, 30);
    visualizer.setBounds (180, 92, 400, 328);
    speed.setBounds (52, 118, 104, 104);
    depth.setBounds (52, 276, 104, 104);
    quality.setBounds (604, 118, 104, 104);
    edge.setBounds (604, 276, 104, 104);
    voices.setBounds (juce::Rectangle<int> (92, 92).withCentre (visualizer.getBounds().getCentre()));

    // Tabs row
    inputTab.setBounds (18, kTabsY, 128, 30);
    analogTab.setBounds (156, kTabsY, 140, 30);
    shape.setBounds (566, kTabsY, 86, 30);
    wide.setBounds (664, kTabsY, 80, 30);

    // Panels
    const auto panelHeight = kStatusY - kPanelsY - 1;
    const auto third = (getWidth() - 2) / 3;
    aliasingPanel.setBounds (0, kPanelsY, third, panelHeight);
    stabilityPanel.setBounds (third + 1, kPanelsY, third, panelHeight);
    companderPanel.setBounds (2 * third + 2, kPanelsY, getWidth() - 2 * third - 2, panelHeight);
    inputPanel.setBounds (0, kPanelsY, getWidth(), panelHeight);

    sizeButton.setBounds (getWidth() - 30 - 52 - 62, kStatusY + 8, 52, 22);
}

//==============================================================================
void MainView::setStatus (const juce::String& id)
{
    if (id == "preset")       { statusName = "Preset"; statusText = "Browse the factory presets"; }
    else if (id == "ab")      { statusName = "A/B";    statusText = "Compare two settings: switch between slot A and slot B"; }
    else if (id == "size")    { statusName = "Size";   statusText = "Interface size - you can also drag the bottom-right corner"; }
    else if (const auto info = Parameters::infoFor (id); info.name.isNotEmpty())
    {
        statusName = info.name;
        statusText = info.description;
    }
    else
    {
        statusName = {};
        statusText = kDefaultStatus;
    }

    repaint (0, kStatusY, getWidth(), getHeight() - kStatusY);
}

void MainView::mouseEnter (const juce::MouseEvent& e)
{
    for (auto* c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
        if (c->getComponentID().isNotEmpty())
        {
            setStatus (c->getComponentID());
            return;
        }
}

void MainView::mouseExit (const juce::MouseEvent&)
{
    // Exit always precedes the next enter, which sets the new text.
    setStatus ({});
}

//==============================================================================
void MainView::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& presets = factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        menu.addItem (i + 1, presets[(std::size_t) i].name, true, i == processor.getCurrentProgram());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [safe = juce::Component::SafePointer<MainView> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->processor.setCurrentProgram (result - 1);
                        });
}

void MainView::showSizeMenu()
{
    static constexpr std::array<int, 7> sizes { 50, 75, 100, 125, 150, 175, 200 };
    const auto current = getCurrentScale != nullptr ? getCurrentScale() : 1.0f;

    juce::PopupMenu menu;
    for (const auto pct : sizes)
        menu.addItem (pct, juce::String (pct) + "%", true, juce::roundToInt (current * 100.0f) == pct);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sizeButton),
                        [safe = juce::Component::SafePointer<MainView> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0 && safe->onScaleChosen != nullptr)
                                safe->onScaleChosen ((float) result / 100.0f);
                        });
}

//==============================================================================
void MainView::tick (double elapsedSeconds, bool refreshPanels)
{
    const auto params = snapshot.read();
    const auto digital = params.engine == reckless::Engine::Digital;

    if (digital != lastDigital)
    {
        lastDigital = digital;
        lnf.setAccent (Palette::accentFor (digital));
        for (auto* c : std::initializer_list<juce::Component*> { &aliasingPanel, &stabilityPanel, &companderPanel })
            c->setAlpha (digital ? 0.35f : 1.0f);
        repaint();
    }

    visualizer.update (processor.getLfoPhase(), params, elapsedSeconds);

    if (refreshPanels)
    {
        for (auto* panel : std::initializer_list<GraphPanel*> { &inputPanel, &aliasingPanel, &stabilityPanel, &companderPanel })
            if (panel->isVisible())
                panel->refresh (params);

        const auto name = processor.getProgramName (processor.getCurrentProgram())
                        + (processor.isPresetModified() ? " *" : "");
        presetName.setText (name);
        abButton.setText (processor.getActiveSlot() == 0 ? "A" : "B");
        abButton.setSelected (processor.getActiveSlot() == 1);

        if (getCurrentScale != nullptr)
            if (const auto scale = getCurrentScale(); ! juce::approximatelyEqual (scale, lastScale))
            {
                lastScale = scale;
                sizeButton.setText (juce::String (juce::roundToInt (scale * 100.0f)) + "%");
            }
    }
}

//==============================================================================
RecklessChorusEditor::RecklessChorusEditor (RecklessChorusProcessor& p)
    : AudioProcessorEditor (&p), chorusProcessor (p), view (p, lnf)
{
    // Read before any resize: resized() overwrites the stored width.
    const auto savedWidth = chorusProcessor.getEditorWidth();

    setLookAndFeel (&lnf);
    addAndMakeVisible (view);

    view.onScaleChosen = [this] (float s) { setScale (s); };
    view.getCurrentScale = [this] { return (float) getWidth() / (float) MainView::kWidth; };

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (MainView::kWidth * kMinScale), juce::roundToInt (MainView::kHeight * kMinScale),
                     juce::roundToInt (MainView::kWidth * kMaxScale), juce::roundToInt (MainView::kHeight * kMaxScale));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) MainView::kWidth / (double) MainView::kHeight);

    setScale ((float) savedWidth / (float) MainView::kWidth);

    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

RecklessChorusEditor::~RecklessChorusEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RecklessChorusEditor::setScale (float scale)
{
    scale = juce::jlimit (kMinScale, kMaxScale, scale);
    setSize (juce::roundToInt (MainView::kWidth * scale), juce::roundToInt (MainView::kHeight * scale));
}

void RecklessChorusEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);
}

void RecklessChorusEditor::resized()
{
    const auto scale = (float) getWidth() / (float) MainView::kWidth;
    view.setBounds (0, 0, MainView::kWidth, MainView::kHeight);
    view.setTransform (juce::AffineTransform::scale (scale));
    chorusProcessor.setEditorWidth (getWidth());
}

void RecklessChorusEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto elapsed = juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001);
    lastTick = now;

    view.tick (elapsed, (++frame & 1) == 0);
}
