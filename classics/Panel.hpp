#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include <numbers>

namespace classics {
using juce::Colour;
using juce::Graphics;
using juce::Rectangle;
using juce::String;

inline constexpr float pi=std::numbers::pi_v<float>;
inline const Colour ink{0xffe7e4d8};
inline const Colour orange{0xffc48254};

inline void label(Graphics& g,const String& text,float x,float y,float w,float h,
                  float size,Colour colour=ink) {
    g.setColour(colour);
    g.setFont(juce::Font(juce::FontOptions(size)));
    g.drawFittedText(text,Rectangle<float>{x,y,w,h}.toNearestInt(),
                     juce::Justification::centred,2);
}

inline juce::Point<float> polar(float x,float y,float radius,float angle) {
    return {x+radius*std::sin(angle),y-radius*std::cos(angle)};
}

inline void marks(Graphics& g,float x,float y,float radius,float start,float finish,
                  const juce::StringArray& texts,Colour colour=ink,float font=12) {
    for (int i=0;i<texts.size();++i) {
        const auto angle=start+(finish-start)*float(i)/float(texts.size()-1);
        const auto mark=polar(x,y,radius,angle);
        g.setColour(ink);
        g.fillEllipse(mark.x-2,mark.y-2,4,4);
        const auto tick=polar(x,y,radius+12,angle);
        if (texts[i].isNotEmpty()) label(g,texts[i],tick.x-21,tick.y-8,42,16,font,colour);
    }
}

class Rotary final : public juce::Slider {
public:
    enum class Type { skirt, ring, centre };

    Rotary(Type style,float radius,float first,float last)
        : juce::Slider(juce::Slider::RotaryVerticalDrag,juce::Slider::NoTextBox),
          type(style),rim(radius),start(first),end(last) {
        setRotaryParameters(first+2*pi,last+2*pi,true);
        setMouseDragSensitivity(140);
    }

    bool hitTest(int x,int y) override {
        const auto d=std::hypot(float(x)-getWidth()*.5f,float(y)-getHeight()*.5f);
        return d<=rim && (type!=Type::ring || d>=rim*.68f);
    }

    void paint(Graphics& g) override {
        const float x=getWidth()*.5f,y=getHeight()*.5f;
        const float position=float(valueToProportionOfLength(getValue()));
        const float angle=start+(end-start)*position;
        const bool metal=type==Type::ring;
        const auto bounds=[&](float r) {return Rectangle<float>{x-r,y-r,2*r,2*r};};
        g.setColour(Colour{0xc5000000});
        g.fillEllipse(bounds(rim).translated(1,2));
        g.setGradientFill(juce::ColourGradient(metal?Colour{0xffaaa392}:Colour{0xff616058},
            x-rim,y-rim,Colour{0xff181918},x+rim,y+rim,false));
        g.fillEllipse(bounds(rim));
        if (type==Type::skirt) {
            g.setColour(Colour{0xff242523});
            g.fillEllipse(bounds(rim*.82f));
            g.setColour(Colour{0xff0b0b0b});
            for (int n=0;n<28;++n) {
                const auto a=float(n)*2*pi/28;
                g.drawLine({polar(x,y,rim*.70f,a),polar(x,y,rim*.82f,a)},1);
            }
            juce::Path grip;
            grip.addQuadrilateral(x-rim*.19f,y+rim*.46f, x+rim*.19f,y+rim*.46f,
                                  x+rim*.24f,y+rim*.96f, x-rim*.24f,y+rim*.96f);
            grip.applyTransform(juce::AffineTransform::rotation(angle,x,y));
            g.setColour(Colour{0xff3e403c});
            g.fillPath(grip);
        }
        if (!metal) {
            const float cap=type==Type::skirt ? .68f : .82f;
            g.setGradientFill(juce::ColourGradient(Colour{0xff55554f},x,y-rim,
                Colour{0xff222320},x,y+rim,false));
            g.fillEllipse(bounds(rim*cap));
            g.setColour(Colour{0xff111210});
            g.drawEllipse(bounds(rim*cap),1);
        }
        g.setColour(metal?Colour{0xff1e1f1a}:ink);
        g.drawLine({polar(x,y,rim*(metal?.72f:.47f),angle),
                    polar(x,y,rim*.95f,angle)},type==Type::skirt?2.4f:1.9f);
    }

private:
    Type type;
    float rim,start,end;
};

class SwitchLook final : public juce::LookAndFeel_V4 {
public:
    void drawToggleButton(Graphics& g,juce::ToggleButton& b,bool over,bool) override {
        const auto box=b.getLocalBounds().toFloat();
        const auto centre=box.getCentre();
        const bool on=b.getToggleState();
        const auto type=b.getComponentID();
        if (type=="rocker") {
            const auto recess=box.reduced(1,2);
            g.setColour(Colour{0xff101111});
            g.fillRoundedRectangle(recess,3);
            g.setGradientFill(juce::ColourGradient(Colour{0xff6a6b64},centre.x,recess.getY(),
                Colour{0xff181918},centre.x,recess.getBottom(),false));
            g.fillRoundedRectangle(recess.reduced(4,5).translated(0,on?2.f:-2.f),2);
            g.setColour(Colour{0xff242525});
            g.fillRoundedRectangle({recess.getX()+7,on?recess.getBottom()-9:recess.getY()+6,
                                   recess.getWidth()-14,2},1);
        } else {
            const float r=std::min(box.getWidth(),box.getHeight())*.42f;
            const Rectangle<float> ring{centre.x-r,centre.y-r,2*r,2*r};
            g.setColour(Colour{0xff0b0c0b});
            g.fillEllipse(ring.expanded(1));
            g.setGradientFill(juce::ColourGradient(Colour{0xffdedbd0},centre.x-r,centre.y-r,
                Colour{0xff4a4942},centre.x+r,centre.y+r,false));
            g.fillEllipse(ring);
            g.setColour(Colour{0xff262522});
            g.fillEllipse(ring.reduced(r*.32f));
            const auto tip=centre.translated(type=="side" ? (on?-r*.68f:r*.68f) : 0,
                                              type=="side" ? 0 : (on?r*.68f:-r*.68f));
            g.setColour(Colour{0xffa8a79e});
            g.drawLine({centre,tip},r*.54f);
            g.setColour(Colour{0xffdbd9c9});
            g.fillEllipse(tip.x-r*.30f,tip.y-r*.3f,r*.60f,r*.60f);
        }
        if (over || b.hasKeyboardFocus(true)) {
            g.setColour(ink.withAlpha(.32f));
            g.drawRoundedRectangle(box.reduced(1),3,1);
        }
    }
};

inline void screw(Graphics& g,float x,float y,float radius) {
    const Rectangle<float> area{x-radius,y-radius,2*radius,2*radius};
    g.setColour(Colour{0xff171818});
    g.fillEllipse(area.expanded(1));
    g.setGradientFill(juce::ColourGradient(Colour{0xff7d7a70},x-radius,y-radius,
        Colour{0xff343530},x+radius,y+radius,false));
    g.fillEllipse(area);
    g.setColour(Colour{0xff242425});
    g.drawLine(x-radius*.48f,y+radius*.36f,x+radius*.56f,y-radius*.41f,1.6f);
}
}
