#pragma once

#include <cstdint>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>
#include <variant>

namespace svg
{
    using namespace std::literals;

    struct Point {
        Point() = default;
        Point(double x, double y)
            : x(x)
            , y(y) {
        }
        double x = 0;
        double y = 0;
    };

    std::string EscapeXMLSymbols(const std::string& arg_str);

    /*
     * Вспомогательная структура, хранящая контекст для вывода SVG-документа с отступами.
     * Хранит ссылку на поток вывода, текущее значение и шаг отступа при выводе элемента
     */
    struct RenderContext
    {
        RenderContext(std::ostream& out) : out(out)
        {}

        RenderContext(std::ostream& out, int indent_step, int indent = 0)
            : out(out), indent_step(indent_step), indent(indent)
        {}

        RenderContext Indented() const
        {
            return {out, indent_step, indent + indent_step};
        }

        void RenderIndent() const
        {
            for (int i = 0; i < indent; ++i)
                out.put(' ');
        }

        std::ostream& out;
        int indent_step = 0;
        int indent = 0;
    };

    /*
     * Абстрактный базовый класс Object служит для унифицированного хранения
     * конкретных тегов SVG-документа
     * Реализует паттерн "Шаблонный метод" для вывода содержимого тега
     */
    class Object
    {
    public:
        void Render(const RenderContext& context) const;
        virtual ~Object() = default;

    private:
        virtual void RenderObject(const RenderContext& context) const = 0;
    };

    struct Rgb
    {
        Rgb(uint8_t red_ = 0, uint8_t green_ = 0, uint8_t blue_ = 0) :
            red(red_), green(green_), blue(blue_)
        {}

        uint8_t red;
        uint8_t green;
        uint8_t blue;
    };

    struct Rgba
    {
        Rgba(uint8_t red_ = 0, uint8_t green_ = 0, uint8_t blue_ = 0, double opacity_ = 1) :
            red(red_), green(green_), blue(blue_), opacity(opacity_)
        {}

        uint8_t red;
        uint8_t green;
        uint8_t blue;
        double opacity;
    };

    using Color = std::variant<std::monostate, std::string, Rgb, Rgba>;

    struct ColorHandler
    {
        std::string operator()(std::monostate);
        std::string operator()(const Rgb& rgb);
        std::string operator()(const Rgba& rgba);
        std::string operator()(const std::string& string_color);
    };

    inline const std::string NoneColor = "none";
    inline const std::string line_cap_value_[] = {"", "butt", "round", "square"};
    inline const std::string line_join_value_[] =
        {"", "arcs", "bevel", "miter", "miter-clip", "round"};

    enum class StrokeLineCap
    {
        NO_LINE_CAP = 0,
        BUTT,
        ROUND,
        SQUARE
    };

    enum class StrokeLineJoin
    {
        NO_LINE_JOIN = 0,
        ARCS,
        BEVEL,
        MITER,
        MITER_CLIP,
        ROUND
    };

    std::ostream& operator<<(std::ostream& out, StrokeLineCap stc);
    std::ostream& operator<<(std::ostream& out, StrokeLineJoin slj);

    template <typename Owner>
    class PathProps
    {
    public:
        PathProps() = default;

        Owner& SetFillColor(Color color)
        {
            fill_color_ = std::move(color);
            return static_cast<Owner&>(*this);
        }

        Owner& SetStrokeColor(Color color)
        {
            stroke_color_ = std::move(color);
            return static_cast<Owner&>(*this);
        }

        Owner& SetStrokeWidth(double width)
        {
            width_ = width;
            return static_cast<Owner&>(*this);
        }

        Owner& SetStrokeLineCap(StrokeLineCap line_cap)
        {
            line_cap_ = line_cap;
            return static_cast<Owner&>(*this);
        }

        Owner& SetStrokeLineJoin(StrokeLineJoin line_join)
        {
            line_join_ = line_join;
            return static_cast<Owner&>(*this);
        }

    protected:
        ~PathProps() = default;

        void RenderAttrs(std::ostream& out) const
        {
            const std::string fill_col = std::visit(ColorHandler{}, fill_color_);
            const std::string stroke_col = std::visit(ColorHandler{}, stroke_color_);

            if (fill_col.size())
                out << " fill=\"" << fill_col << "\"";
            if (stroke_col.size())
                out << " stroke=\"" << stroke_col << "\"";
            if (width_ > 0)
                out << " stroke-width=\"" << width_ << "\"";
            if (line_cap_ != StrokeLineCap::NO_LINE_CAP)
                out << " stroke-linecap=\"" << line_cap_value_[static_cast<int>(line_cap_)] << "\"";
            if (line_join_ != StrokeLineJoin::NO_LINE_JOIN)
                out << " stroke-linejoin=\"" << line_join_value_[static_cast<int>(line_join_)] << "\"";
        }

    private:

        Color fill_color_;
        Color stroke_color_;
        double width_ = -1;
        StrokeLineCap line_cap_ = StrokeLineCap::NO_LINE_CAP;
        StrokeLineJoin line_join_ = StrokeLineJoin::NO_LINE_JOIN;
    };

    /*
     * Класс Circle моделирует элемент <circle> для отображения круга
     * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/circle
     */
    class Circle final : public Object, public PathProps<Circle>
    {
    public:
        Circle& SetCenter(Point center);
        Circle& SetRadius(double radius);

    private:
        void RenderObject(const RenderContext& context) const override;

        Point center_;
        double radius_ = 1.0;
    };

    /*
     * Класс Polyline моделирует элемент <polyline> для отображения ломаных линий
     * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/polyline
     */
    class Polyline final : public Object, public PathProps<Polyline>
    {
    public:
        // Добавляет очередную вершину к ломаной линии
        Polyline& AddPoint(Point point);

    private:
        virtual void RenderObject(const RenderContext& context) const override;

        std::vector<Point> poly_points_; //Массив точек ломаной
    };

    /*
     * Класс Text моделирует элемент <text> для отображения текста
     * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/text
     */
    class Text final : public Object, public PathProps<Text>
    {
    public:
        // Задаёт координаты опорной точки (атрибуты x и y)
        Text& SetPosition(Point pos);

        // Задаёт смещение относительно опорной точки (атрибуты dx, dy)
        Text& SetOffset(Point offset);

        // Задаёт размеры шрифта (атрибут font-size)
        Text& SetFontSize(uint32_t size);

        // Задаёт название шрифта (атрибут font-family)
        Text& SetFontFamily(const std::string& font_family);

        // Задаёт толщину шрифта (атрибут font-weight)
        Text& SetFontWeight(const std::string& font_weight);

        // Задаёт текстовое содержимое объекта (отображается внутри тега text)
        Text& SetData(const std::string& data);

    private:
        virtual void RenderObject(const RenderContext& context) const override;

        Point pos_ = {0, 0}; // Координаты опорной точки (атрибуты x и y)
        Point offset_ = {0, 0}; // Смещение относительно опорной точки (атрибуты dx, dy)
        uint32_t font_size_ = 1; // Размеры шрифта (атрибут font-size)
        std::string font_family_; // Название шрифта (атрибут font-family)
        std::string font_weight_; // Толщина шрифта (атрибут font-weight)
        std::string data_; // Текстовое содержимое объекта (отображается внутри тега text)
    };

    class ObjectContainer
    {
    public:
        virtual ~ObjectContainer()
        {}

        // Добавляет в svg-документ объект-наследник svg::Object
        virtual void AddPtr(std::unique_ptr<Object>&& obj) = 0;

        template <typename T>
        void Add(const T& object)
        {
            AddPtr(std::make_unique<T>(object));
        }
    };

    class Document : public ObjectContainer
    {
    public:
        // Добавляет в svg-документ объект-наследник svg::Object
        virtual void AddPtr(std::unique_ptr<Object>&& obj) override;
        void Render(std::ostream& out) const;

    private:
        std::vector<std::shared_ptr<Object>> objects_list_;
    };

    class Drawable
    {
    public:
        virtual ~Drawable()
        {}

        virtual void Draw(ObjectContainer& container) const = 0;
    };

}  // namespace svg
