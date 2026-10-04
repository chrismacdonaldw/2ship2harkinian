#ifndef MM_ANCHOR_H
#define MM_ANCHOR_H
#ifdef ENABLE_ANCHOR

#include "2s2h/Network/Network.h"
#include "PermanentProgress.h"
#include <array>
#include <deque>

class Anchor : public Network {
  public:
    struct Scope {
        char seed[65] = {};
        uint64_t ownerGeneration = 0;
        bool admitted = false;
        bool mmActive = false;
    };
    using ScopeProvider = bool (*)(Scope*);
    using ReadRetained = bool (*)(const Scope*, AnchorProgress::State*);
    using CommitRetained = bool (*)(const Scope*, const AnchorProgress::State*);

    static Anchor* Instance;
    static void Init();
    static void Shutdown();
    // In module mode these callbacks are the sole transport/admission owner (no native socket).
    // read returns false through its baseline/I/O barrier; commit persists local state or fails for retry.
    // All calls are on the game thread; nullptr bindings mean unsupported. No STL crosses this seam.
    void SetSessionCallbacks(ScopeProvider provider, ReadRetained read, CommitRetained commit);
    bool Connect();
    void Disconnect();
    void Pump();
    // One bounded admission attempt while the owning context is valid. False retains pending edits.
    bool FlushSessionEdits();
    bool SetSharing(bool on);
    const char* Status() const;
    bool CanSetSharing() const;
    bool Sharing() const {
        return sharing;
    }

  private:
    using Json = nlohmann::json;
    ScopeProvider scopeProvider = nullptr;
    ReadRetained readRetained = nullptr;
    CommitRetained commitRetained = nullptr;
    Scope scope;
    std::string room, team, epoch, nonce, nativeSeed;
    uint64_t generation = 0, clientId = 0, ownerClientId = 0, sequence = 0;
    uint64_t nativeLoadGeneration = 0, scopedLoadGeneration = UINT64_MAX, scopedOwnerGeneration = UINT64_MAX;
    uint8_t scopedFile = 0xFF;
    std::string scopedProfile;
    uint32_t request = 0, requestSerial = 0;
    size_t awaitingLocalEcho = 0;
    unsigned capability = 0;
    bool sharing = false, authoritative = false, pendingApply = false, refreshOwner = true;
    bool localQueueFailed = false;
    AnchorProgress::State retained;
    std::deque<AnchorProgress::Edit> localEdits;
    std::deque<Json> incomingEdits;
    struct Stage {
        uint64_t sequence = 0;
        bool known = false;
        size_t bytes = 0;
        std::vector<Json> pages;
    } stage;
    double lastHello = -10, lastRequest = -10, lastLocalSend = -10;

    bool GetScope(Scope& next);
    void RememberNativeOwner(const Scope& checked);
    bool SameNativeOwner() const;
    bool Send(Json packet);
    Json Envelope(const char* type) const;
    Json ClientState() const;
    void Handshake();
    void ResetProtocol();
    void RequestState();
    void SendBaseline(uint32_t token);
    void Receive(const Json& packet);
    bool FinishState();
    bool Commit(const AnchorProgress::State& state);
    void FlushLocal();
    void LocalEdit(const AnchorProgress::Edit& edit);
};

void AnchorMenu();
#endif
#endif
