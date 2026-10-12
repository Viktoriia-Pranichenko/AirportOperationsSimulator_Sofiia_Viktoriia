#pragma once

class BaggageTug
{
public:
    BaggageTug(float positionX, float positionZ);

    void Draw() const;

private:
    float positionX;
    float positionZ;
};
