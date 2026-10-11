#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Graphite panels, silver text, Audition-blue accent.
namespace apollo
{
    namespace colours
    {
        const juce::Colour black      { 0xff08090b };
        const juce::Colour panel      { 0xff121317 };
        const juce::Colour raised     { 0xff1b1d22 };
        const juce::Colour hover      { 0xff25282f };
        const juce::Colour edge       { 0xff2e3138 };
        const juce::Colour grid       { 0xff1f2229 };
        const juce::Colour textDim    { 0xff7c828c };
        const juce::Colour text       { 0xffc4c9d1 };
        const juce::Colour bright     { 0xfff2f4f7 };
        const juce::Colour accent     { 0xff4f95ff };
        const juce::Colour accentSoft { 0xff8fbaff };
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            using namespace colours;
            setColourScheme ({ panel, black, raised, edge, text, raised, black, accent, text });

            setColour (juce::ResizableWindow::backgroundColourId, black);
            setColour (juce::TextButton::buttonColourId, raised);
            setColour (juce::TextButton::buttonOnColourId, accent);
            setColour (juce::TextButton::textColourOffId, textDim);
            setColour (juce::TextButton::textColourOnId, bright);
            setColour (juce::ToggleButton::textColourId, text);
            setColour (juce::ComboBox::backgroundColourId, black);
            setColour (juce::ComboBox::textColourId, text);
            setColour (juce::ComboBox::outlineColourId, edge);
            setColour (juce::ComboBox::arrowColourId, textDim);
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141518).withAlpha (0.99f)); // not opaque: lets the corners round
            setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, hover);
            setColour (juce::PopupMenu::highlightedTextColourId, bright);
            setColour (juce::Label::textColourId, textDim);
            setColour (juce::TextEditor::backgroundColourId, black);
            setColour (juce::TextEditor::textColourId, bright);
            setColour (juce::TextEditor::outlineColourId, accent);
            setColour (juce::TextEditor::focusedOutlineColourId, accent);
            setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.45f));
            setColour (juce::CaretComponent::caretColourId, bright);
            setColour (juce::AlertWindow::backgroundColourId, raised);
            setColour (juce::AlertWindow::textColourId, text);
            setColour (juce::AlertWindow::outlineColourId, edge);
            setColour (juce::TooltipWindow::backgroundColourId, raised);
            setColour (juce::TooltipWindow::textColourId, text);
            setColour (juce::TooltipWindow::outlineColourId, edge);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                   bool highlighted, bool down) override
        {
            using namespace colours;
            const auto r = button.getLocalBounds().toFloat().reduced (0.5f);
            const bool on = button.getToggleState();
            const float alpha = button.isEnabled() ? 1.0f : 0.35f;

            if (on)
                g.setGradientFill (juce::ColourGradient (accent.brighter (0.15f), 0.0f, r.getY(), accent.darker (0.35f), 0.0f, r.getBottom(), false));
            else
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff23252a), 0.0f, r.getY(), juce::Colour (0xff111215), 0.0f, r.getBottom(), false));

            g.setOpacity (alpha);
            g.fillRoundedRectangle (r, 4.0f);

            g.setColour (juce::Colours::white.withAlpha ((on ? 0.18f : 0.06f) * alpha)); // top sheen
            g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);

            if (down)
            {
                g.setColour (juce::Colours::black.withAlpha (0.25f));
                g.fillRoundedRectangle (r, 4.0f);
            }

            g.setColour ((on ? accent.brighter (0.3f) : (highlighted && button.isEnabled() ? textDim : edge)).withAlpha (alpha));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::Font (juce::FontOptions ((float) std::min (13, buttonHeight - 9), juce::Font::bold));
        }

        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool) override
        {
            using namespace colours;
            const auto bounds = button.getLocalBounds().toFloat();
            const auto box = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ bounds.getX() + 9.0f, bounds.getCentreY() });
            const bool on = button.getToggleState();

            g.setColour (on ? accent : black);
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (on ? accent.brighter (0.3f) : (highlighted ? textDim : edge));
            g.drawRoundedRectangle (box, 3.0f, 1.0f);

            if (on)
            {
                juce::Path tick;
                tick.startNewSubPath (box.getX() + 3.5f, box.getCentreY());
                tick.lineTo (box.getX() + 6.0f, box.getBottom() - 4.0f);
                tick.lineTo (box.getRight() - 3.0f, box.getY() + 3.5f);
                g.setColour (bright);
                g.strokePath (tick, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            g.setColour (button.findColour (juce::ToggleButton::textColourId));
            g.setFont (juce::FontOptions (13.0f));
            g.drawText (button.getButtonText(), bounds.withTrimmedLeft (22.0f), juce::Justification::centredLeft);
        }

        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                               float, const juce::Slider::SliderStyle style, juce::Slider& slider) override
        {
            using namespace colours;

            if (style != juce::Slider::LinearVertical)
                return LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, slider);

            const float cx = (float) x + (float) width * 0.5f;
            const juce::Rectangle<float> track (cx - 1.5f, (float) y, 3.0f, (float) height);
            const float zeroY = (float) slider.getPositionOfValue (0.0);

            g.setColour (edge);
            g.fillRoundedRectangle (track, 1.5f);
            g.setColour (accent);
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (cx - 1.5f, std::min (zeroY, sliderPos), cx + 1.5f, std::max (zeroY, sliderPos)));

            const auto thumb = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ cx, sliderPos });
            g.setColour (black);
            g.fillEllipse (thumb);
            g.setColour (slider.isMouseOverOrDragging() ? bright : text);
            g.drawEllipse (thumb.reduced (1.0f), 2.2f);
        }

        void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
        {
            using namespace colours;
            const auto r = juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f);
            g.setColour (box.isMouseOver (true) ? raised : black);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (box.hasKeyboardFocus (true) ? accent : edge);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);

            const float ax = r.getRight() - 12.0f, ay = r.getCentreY();
            juce::Path arrow;
            arrow.startNewSubPath (ax - 3.5f, ay - 1.5f);
            arrow.lineTo (ax, ay + 2.0f);
            arrow.lineTo (ax + 3.5f, ay - 1.5f);
            g.setColour (textDim);
            g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
            label.setFont (getComboBoxFont (box));
        }

        juce::Font getComboBoxFont (juce::ComboBox& box) override { return juce::Font (juce::FontOptions (box.getHeight() < 24 ? 12.5f : 13.5f)); }
        juce::Font getLabelFont (juce::Label&) override           { return juce::Font (juce::FontOptions (12.5f)); }

        //==============================================================================
        // Menus: a rounded dark panel, roomy rows, a soft rounded highlight and a check mark.
        void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
        {
            const auto r = juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f);
            g.setColour (juce::Colour (0xff141518));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (colours::edge);
            g.drawRoundedRectangle (r, 8.0f, 1.0f);
        }

        int getPopupMenuBorderSize() override { return 6; }

        juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (13.5f)); }

        void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int, int& idealWidth, int& idealHeight) override
        {
            if (isSeparator)
            {
                idealWidth = 60;
                idealHeight = 9;
                return;
            }

            idealHeight = 28;
            idealWidth = juce::GlyphArrangement::getStringWidthInt (getPopupMenuFont(), text) + 64;
        }

        void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                const juce::String& shortcutKeyText, const juce::Drawable*, const juce::Colour*) override
        {
            using namespace colours;

            if (isSeparator)
            {
                g.setColour (edge);
                g.fillRect (area.reduced (10, 0).withHeight (1).withCentre (area.getCentre()));
                return;
            }

            const auto row = area.toFloat().reduced (2.0f, 1.0f);

            if (isHighlighted && isActive)
            {
                g.setColour (hover);
                g.fillRoundedRectangle (row, 5.0f);
            }

            if (isTicked)
            {
                const float cx = row.getX() + 15.0f, cy = row.getCentreY();
                juce::Path tick;
                tick.startNewSubPath (cx - 4.5f, cy + 0.5f);
                tick.lineTo (cx - 1.0f, cy + 4.0f);
                tick.lineTo (cx + 5.0f, cy - 4.0f);
                g.setColour (accentSoft);
                g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            g.setColour ((isHighlighted ? bright : colours::text).withAlpha (isActive ? 1.0f : 0.35f));
            g.setFont (getPopupMenuFont());
            const auto textArea = row.withTrimmedLeft (30.0f).withTrimmedRight (12.0f).toNearestInt();
            g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);

            if (shortcutKeyText.isNotEmpty())
            {
                g.setColour (textDim);
                g.setFont (juce::FontOptions (12.0f));
                g.drawText (shortcutKeyText, textArea, juce::Justification::centredRight);
            }

            if (hasSubMenu)
            {
                const float cx = row.getRight() - 12.0f, cy = row.getCentreY();
                juce::Path arrow;
                arrow.startNewSubPath (cx - 1.5f, cy - 3.5f);
                arrow.lineTo (cx + 2.0f, cy);
                arrow.lineTo (cx - 1.5f, cy + 3.5f);
                g.setColour (textDim);
                g.strokePath (arrow, juce::PathStrokeType (1.4f));
            }
        }

        void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& text) override
        {
            g.setColour (colours::textDim);
            g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.08f));
            g.drawText (text.toUpperCase(), area.withTrimmedLeft (14).withTrimmedTop (4), juce::Justification::centredLeft);
        }
    };
}
