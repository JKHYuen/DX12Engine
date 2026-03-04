#pragma once

class Scene;
class GameObject;

class Picker {
public:
	Picker(Scene* scene);

	bool Raycast(int mouseX, int mouseY, int windowWidth, int windowHeight);

private:
	Scene* m_Scene {};
	GameObject* m_PickedObject {};
};

