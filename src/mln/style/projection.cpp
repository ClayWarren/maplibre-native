#include <mln/style/projection.hpp>
#include <mln/style/projection_impl.hpp>
#include <mln/style/projection_observer.hpp>
#include <mln/style/conversion/property_value.hpp>
#include <mln/style/conversion_impl.hpp>
#include <mln/renderer/property_evaluator.hpp>

namespace mln {
namespace style {

namespace {
ProjectionObserver nullObserver;
} // namespace

Projection::Projection(Immutable<Projection::Impl> impl_)
    : impl(std::move(impl_)),
      observer(&nullObserver) {}

Projection::Projection()
    : Projection(makeMutable<Impl>()) {}

Projection::~Projection() = default;

void Projection::setObserver(ProjectionObserver* observer_) {
    observer = observer_ ? observer_ : &nullObserver;
}

Mutable<Projection::Impl> Projection::mutableImpl() const {
    return makeMutable<Impl>(*impl);
}

ProjectionDefinition Projection::getDefaultType() {
    return ProjectionDefinition();
}

PropertyValue<ProjectionDefinition> Projection::getType() const {
    return impl->type;
}

void Projection::setType(PropertyValue<ProjectionDefinition> type) {
    auto mutableImpl_ = mutableImpl();
    mutableImpl_->type = std::move(type);
    impl = std::move(mutableImpl_);
    observer->onProjectionChanged(*this);
}

std::optional<conversion::Error> Projection::setProperty(const std::string& name,
                                                         const conversion::Convertible& value) {
    if (name != "type") {
        return conversion::Error{"layer doesn't support this property"};
    }
    conversion::Error error;
    const auto type = conversion::convert<PropertyValue<ProjectionDefinition>>(value, error, false, false);
    if (!type) {
        return error;
    }
    setType(*type);
    return std::nullopt;
}

StyleProperty Projection::getProperty(const std::string& name) const {
    if (name == "type") {
        return conversion::makeStyleProperty(getType());
    }
    return {};
}

ProjectionDefinition Projection::Impl::evaluate(float zoom) const {
    const PropertyEvaluationParameters parameters(zoom);
    const auto definition = type.evaluate(
        PropertyEvaluator<ProjectionDefinition>(parameters, Projection::getDefaultType()));
    // `globe` is the vertical perspective up to zoom 11 and Mercator from zoom 12, blended in between.
    if (definition.from == "globe" && definition.to == "globe") {
        if (zoom <= 11) {
            return ProjectionDefinition("vertical-perspective");
        }
        if (zoom >= 12) {
            return ProjectionDefinition("mercator");
        }
        return ProjectionDefinition("vertical-perspective", "mercator", zoom - 11);
    }
    return definition;
}

} // namespace style
} // namespace mln
