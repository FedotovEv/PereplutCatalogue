#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>
#include <variant>

#include "svg.h"

namespace svg
{
    using namespace std::literals;

    std::string EscapeXMLSymbols(const std::string& arg_str)
    {
        std::string result;
        for (char c : arg_str)
            switch(c)
            {
                case '"':
                    result += "&quot;";
                    break;
                case '\'':
                    result += "&apos;";
                    break;
                case '<':
                    result += "&lt;";
                    break;
                case '>':
                    result += "&gt;";
                    break;
                case '&':
                    result += "&amp;";
                    break;
                default:
                    result += c;
            }
        return result;
    }

    std::ostream& operator<<(std::ostream& out, StrokeLineCap stc)
    {
        out << line_cap_value_[static_cast<int>(stc)];
        return out;
    }

    std::ostream& operator<<(std::ostream& out, StrokeLineJoin slj)
    {
        out << line_join_value_[static_cast<int>(slj)];
        return out;
    }

    // ---------- ColorHandler ------------------

    std::string ColorHandler::operator()(std::monostate)
    {
        return {};
    }

    std::string ColorHandler::operator()(const Rgb& rgb)
    {
        const std::string r = std::to_string(rgb.red);
        const std::string g = std::to_string(rgb.green);
        const std::string b = std::to_string(rgb.blue);

        return "rgb("s + r + ","s + g + ","s + b + ")"s;
    }

    std::string ColorHandler::operator()(const Rgba& rgba)
    {
        const std::string r = std::to_string(rgba.red);
        const std::string g = std::to_string(rgba.green);
        const std::string b = std::to_string(rgba.blue);
        std::ostringstream alpha;
        alpha << rgba.opacity;

        return "rgba("s + r + ","s + g + ","s + b + ","s + alpha.str() + ")"s;
    }

    std::string ColorHandler::operator()(const std::string& string_color)
    {
        return string_color;
    }

    // ---------- Document ------------------

    void Document::AddPtr(std::unique_ptr<Object>&& obj)
    {
        objects_list_.push_back(std::move(obj));
    }

    void Document::Render(std::ostream& out) const
    {
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" << std::endl;
        out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">" << std::endl;

        for (auto& object_ptr : objects_list_)
            object_ptr->Render(out);

        out << "</svg>" << std::endl;
    }

    // ---------- Object ------------------

    void Object::Render(const RenderContext& context) const
    {
        context.RenderIndent();

        // Делегируем вывод тега своим подклассам
        RenderObject(context);

        context.out << std::endl;
    }

    // ---------- Circle ------------------

    Circle& Circle::SetCenter(Point center)
    {
        center_ = center;
        return *this;
    }

    Circle& Circle::SetRadius(double radius)
    {
        radius_ = radius;
        return *this;
    }

    void Circle::RenderObject(const RenderContext& context) const
    {
        auto& out = context.out;
        out << "<circle cx=\"" << center_.x << "\" cy=\"" << center_.y << "\" ";
        out << "r=\"" << radius_ << '"';
        RenderAttrs(out);
        out << "/>";
    }

    // ---------- Polyline ------------------

    Polyline& Polyline::AddPoint(Point point)
    {
        poly_points_.push_back(std::move(point));
        return *this;
    }

    void Polyline::RenderObject(const RenderContext& context) const
    {
        context.out << "<polyline points=\"";
        size_t point_number = 0;
        for (const Point& point : poly_points_)
        {
            context.out << point.x << ',' << point.y;
            if (point_number < poly_points_.size() - 1)
                context.out << ' ';
            ++point_number;
        }
        context.out << '"';
        RenderAttrs(context.out);
        context.out << "/>";
    }

    // ---------- Text ------------------

    Text& Text::SetPosition(Point pos)
    {
        pos_ = pos;
        return *this;
    }

    Text& Text::SetOffset(Point offset)
    {
        offset_ = offset;
        return *this;
    }

    Text& Text::SetFontSize(uint32_t size)
    {
        font_size_ = size;
        return *this;
    }

    Text& Text::SetFontFamily(const std::string& font_family)
    {
        font_family_ = font_family;
        return *this;
    }

    Text& Text::SetFontWeight(const std::string& font_weight)
    {
        font_weight_ = font_weight;
        return *this;
    }

    Text& Text::SetData(const std::string& data)
    {
        data_ = data;
        return *this;
    }

    void Text::RenderObject(const RenderContext& context) const
    {
        context.out << "<text x=\"" << pos_.x << "\" y=\"" << pos_.y << '"';
        context.out << " dx=\"" << offset_.x << "\" dy=\"" << offset_.y << '"';
        if (font_size_ > 0)
            context.out << " font-size=\"" << font_size_ << '"';
        if (font_family_.size() > 0)
            context.out << " font-family=\"" << font_family_ << '"';
        if (font_weight_.size() > 0)
            context.out << " font-weight=\"" << font_weight_ << '"';
        RenderAttrs(context.out);
        context.out << '>';
        context.out << EscapeXMLSymbols(data_) << "</text>";
    }

}  // namespace svg
