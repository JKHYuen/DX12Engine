#pragma once

#include <utility> // for std::pair
#include <vector> 

class Scene;
class GameObject;

class Picker {
public:
	Picker(Scene* scene);

	bool Raycast(int mouseX, int mouseY, int windowWidth, int windowHeight);

	GameObject* GetPickedObject() const { return m_PickedObject; };
	void ClearPickedObject() { m_PickedObject = nullptr; };

private:
	Scene* m_Scene {};
	GameObject* m_PickedObject {};

	std::vector<std::pair<float, GameObject*>> m_RaycastCache {};
	std::pair<int, int> m_LastMousePos {};
};

