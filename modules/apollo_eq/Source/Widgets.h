#pragma once

#include "ApolloLookAndFeel.h"
#include "Params.h"

namespace apollo
{
    /** A number you drag vertically (Shift for fine), click to type, scroll, or Ctrl/Alt-click to reset. */
    class ValueBox : public juce::Component
    {
    public:
        explicit ValueBox (juce::RangedAudioParameter& p)
            : param (p), attachment (p, [this] (float v) { value = v; repaint(); }, nullptr)
        {
            attachment.sendInitialUpdate();
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            setRepaintsOnMouseActivity (true);
        }

        std::function<juce::String (float)> toText;
        std::function<std::optional<float> (const juce::String&)> fromText;
        juce::Colour textColour = colours::accentSoft;
        float fontSize = 14.0f;

        juce::String getText() const { return toText ? toText (value) : param.getCurrentValueAsText(); }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (1.0f);

            if ((isMouseOverOrDragging() || dragging) && isEnabled() && editor == nullptr)
            {
                g.setColour (colours::raised);
                g.fillRoundedRectangle (r, 4.0f);
            }

            g.setColour (textColour);
            g.setFont (juce::FontOptions (fontSize));
            g.drawText (getText(), r.reduced (3.0f, 0.0f), juce::Justification::centred);
        }

        void resized() override
        {
            if (editor != nullptr)
                editor->setBounds (getLocalBounds());
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            dragging = false;
            dragNorm = param.convertTo0to1 (value);
            lastY = 0.0f;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragging)
            {
                if (e.getDistanceFromDragStart() < 3)
                    return;

                dragging = true;
                lastY = e.position.y;
                e.source.enableUnboundedMouseMovement (true);
                attachment.beginGesture();
            }

            const float dy = e.position.y - lastY;
            lastY = e.position.y;
            dragNorm = juce::jlimit (0.0f, 1.0f, dragNorm - dy / (e.mods.isShiftDown() ? 1600.0f : 240.0f));
            attachment.setValueAsPartOfGesture (param.convertFrom0to1 (dragNorm));
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (dragging)
            {
                attachment.endGesture();
                dragging = false;
                e.source.enableUnboundedMouseMovement (false);
                repaint();
            }
            else if (e.mods.isCtrlDown() || e.mods.isAltDown() || e.mods.isCommandDown())
            {
                attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
            }
            else if (! e.mods.isPopupMenu() && isEnabled())
            {
                showEditor();
            }
        }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const float step = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) * (e.mods.isShiftDown() ? 0.01f : 0.05f);
            const float norm = juce::jlimit (0.0f, 1.0f, param.convertTo0to1 (value) + (w.isReversed ? -step : step));
            attachment.setValueAsCompleteGesture (param.convertFrom0to1 (norm));
        }

        void showEditor()
        {
            if (editor != nullptr)
                return;

            editor = std::make_unique<juce::TextEditor>();
            addAndMakeVisible (*editor);
            editor->setBounds (getLocalBounds());
            editor->setJustification (juce::Justification::centred);
            editor->setFont (juce::FontOptions (fontSize));
            editor->setIndents (2, 0);
            editor->setText (getText().upToFirstOccurrenceOf (" ", false, false), false);
            editor->onReturnKey = [this] { closeEditor (true); };
            editor->onEscapeKey = [this] { closeEditor (false); };
            editor->onFocusLost = [this] { closeEditor (true); };
            editor->grabKeyboardFocus();
            editor->selectAll();
        }

    private:
        void closeEditor (bool apply)
        {
            if (editor == nullptr || closing)
                return;

            closing = true;

            if (apply)
            {
                const auto text = editor->getText();
                const auto parsed = fromText ? fromText (text) : std::optional<float> (param.convertFrom0to1 (param.getValueForText (text)));

                if (parsed.has_value())
                    attachment.setValueAsCompleteGesture (param.getNormalisableRange().snapToLegalValue (*parsed));
            }

            // The editor is inside its own callback here, so delete it afterwards.
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ValueBox> (this)]
            {
                if (safe != nullptr)
                {
                    safe->editor.reset();
                    safe->closing = false;
                    safe->repaint();
                }
            });
        }

        juce::RangedAudioParameter& param;
        juce::ParameterAttachment attachment;
        float value = 0.0f, dragNorm = 0.0f, lastY = 0.0f;
        bool dragging = false, closing = false;
        std::unique_ptr<juce::TextEditor> editor;
    };

    //==============================================================================
    /** Two-segment switch bound to a bool parameter (off = left, on = right). */
    class SegmentedSwitch : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        SegmentedSwitch (juce::RangedAudioParameter& p, juce::String offText, juce::String onText)
            : labels { std::move (offText), std::move (onText) },
              attachment (p, [this] (float v) { on = v > 0.5f; repaint(); }, nullptr)
        {
            attachment.sendInitialUpdate();
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& g) override
        {
            using namespace colours;
            const auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (black);
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (edge);
            g.drawRoundedRectangle (r, 5.0f, 1.0f);

            for (int i = 0; i < 2; ++i)
            {
                const auto seg = r.withWidth (r.getWidth() * 0.5f).withX (r.getX() + (float) i * r.getWidth() * 0.5f).reduced (2.0f);
                const bool active = (i == 1) == on;

                if (active)
                {
                    g.setGradientFill (juce::ColourGradient (accent.brighter (0.1f), 0.0f, seg.getY(), accent.darker (0.3f), 0.0f, seg.getBottom(), false));
                    g.fillRoundedRectangle (seg, 3.5f);
                }

                g.setColour (active ? bright : textDim);
                g.setFont (juce::FontOptions (12.5f, active ? juce::Font::bold : juce::Font::plain));
                g.drawText (labels[(size_t) i], seg, juce::Justification::centred);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            attachment.setValueAsCompleteGesture (e.position.x >= (float) getWidth() * 0.5f ? 1.0f : 0.0f);
        }

    private:
        std::array<juce::String, 2> labels;
        juce::ParameterAttachment attachment;
        bool on = false;
    };

    //==============================================================================
    /** Small flat button showing a stroked or filled icon path. */
    class IconButton : public juce::Button
    {
    public:
        IconButton (const juce::String& name, juce::Path p, bool filled = false)
            : Button (name), icon (std::move (p)), fill (filled)
        {
            setTooltip (name);
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            using namespace colours;
            const auto r = getLocalBounds().toFloat().reduced (0.5f);

            if (highlighted && isEnabled())
            {
                g.setColour (down ? hover.brighter (0.1f) : raised);
                g.fillRoundedRectangle (r, 4.0f);
            }

            const auto area = r.reduced (r.getHeight() * 0.27f);
            const auto t = icon.getTransformToScaleToFit (area, true);
            g.setColour ((highlighted ? bright : colours::text).withAlpha (isEnabled() ? 1.0f : 0.3f));

            if (fill)
                g.fillPath (icon, t);
            else
                g.strokePath (icon, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
        }

    private:
        juce::Path icon;
        bool fill;
    };

    namespace icons
    {
        inline juce::Path save()
        {
            juce::Path p; // tray with a down arrow
            p.startNewSubPath (2, 11); p.lineTo (2, 15); p.lineTo (16, 15); p.lineTo (16, 11);
            p.startNewSubPath (9, 1);  p.lineTo (9, 10);
            p.startNewSubPath (5, 6.5f); p.lineTo (9, 10.5f); p.lineTo (13, 6.5f);
            return p;
        }

        inline juce::Path trash()
        {
            juce::Path p;
            p.startNewSubPath (2, 4); p.lineTo (16, 4);
            p.startNewSubPath (6.5f, 4); p.lineTo (7, 1.5f); p.lineTo (11, 1.5f); p.lineTo (11.5f, 4);
            p.startNewSubPath (4, 4); p.lineTo (5, 16); p.lineTo (13, 16); p.lineTo (14, 4);
            p.startNewSubPath (7.5f, 7); p.lineTo (7.5f, 13);
            p.startNewSubPath (10.5f, 7); p.lineTo (10.5f, 13);
            return p;
        }

        inline juce::Path chevron (bool right)
        {
            juce::Path p;
            p.startNewSubPath (right ? 6.0f : 12.0f, 3.0f);
            p.lineTo (right ? 12.0f : 6.0f, 9.0f);
            p.lineTo (right ? 6.0f : 12.0f, 15.0f);
            return p;
        }
    }
}
