#pragma
//-----------------------------------------------------------------------------
/**
    @class Oryol::_priv::tbMgr
    @brief global TurboBadger UI wrapper
*/
#include "Core/Types.h"
#include "Core/RunLoop.h"
#include "IO/Core/IOQueue.h"
#include "TBUI/TBUISetup.h"
#include "TBUI/tb/tbOryolBatchRenderer.h"
#include "TBUI/tb/tbOryolRootWidget.h"
#include "Resource/Core/resourceRegistry.h"
#include "Input/InputProtocol.h"
#include "Messaging/Dispatcher.h"
#include "tb_skin.h"

namespace Oryol {
namespace _priv {

class tbMgr {
public:
    /// destructor
    ~tbMgr();
    
    /// setup the object
    void Setup(const TBUISetup& setup);
    /// discard the object
    void Discard();
    /// return true if object has been setup
    bool IsValid() const;
    /// draw one frame
    void Draw();
    
    /// get pointer to root widget
    tbOryolRootWidget* GetRootWidget();
    
private:
    /// per-frame update
    void update();
    /// mouse move input handler
    void onMouseMove(int posX, int posY);
    /// mouse button input handler
    void onMouseButton(Mouse::Button btn, bool down);
    /// mouse scroll input handler
    void onScroll(int wheelX, int wheelY);
    /// character input handler
    void onWChar(wchar_t c);
    /// key up/down handler
    void onKey(Key::Code key, bool down, bool up);

    bool isValid = false;
    Ptr<Dispatcher<InputProtocol>> inputHandler;
    tb::MODIFIER_KEYS modifierKeys = tb::TB_MODIFIER_NONE;
    int mouseX = 0;
    int mouseY = 0;
    tbOryolBatchRenderer* renderer = nullptr;
    tbOryolRootWidget rootWidget;
};

} // namespace _priv
} // namespace Oryol

