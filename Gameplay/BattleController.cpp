#include "BattleController.h"

#include "BattlePlan.h"
#include "QuantumFog.h"
#include "SquadTemplate.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Potato {
namespace Gameplay {

BattleController::BattleController(int gridWidth, int gridHeight, float cellSize)
    : field(gridWidth, gridHeight, cellSize)
    , phase(BattlePhase::Deployment)
    , outcome(BattleOutcome::Ongoing)
    , timeScale(1.0f)
    , doctrineTimer(0.0f)
    , elapsed(0.0f) {
}

Squad* BattleController::CreateSquad(const std::string& name, int team,
                                     const Vector2& pos, int members) {
    auto squad = std::make_unique<Squad>(name, team, pos, members);
    Squad* ptr = squad.get();
    squads.push_back(std::move(squad));
    Emit(name + " deployed (team " + std::to_string(team) + ")");
    return ptr;
}

Squad* BattleController::CreateSquadFromTemplate(const SquadTemplate& tpl,
                                                 int team,
                                                 const Vector2& pos,
                                                 int& budget) {
    // 規則寫死：超支一律拒絕不降規；budget < 0 = 無限
    if (budget >= 0 && tpl.cost > budget) {
        Emit("template " + tpl.id + " refused: cost " +
             std::to_string(tpl.cost) + " > budget " +
             std::to_string(budget));
        return nullptr;
    }
    Squad* s = tpl.Instantiate(*this, tpl.name, team, pos);
    if (s && budget >= 0) {
        budget -= tpl.cost;
    }
    return s;
}

void BattleController::AssignDoctrine(Squad* squad, const DoctrineSet& doctrine) {
    if (phase != BattlePhase::Deployment || !squad) {
        return;
    }
    doctrines[squad] = doctrine;
    Emit(squad->GetName() + " doctrine assigned (" +
         std::to_string(doctrine.Count()) + " rules)");
}

void BattleController::SetObjective(int team, const Vector2& pos) {
    objectives[team] = pos;
}

void BattleController::SetRallyPoint(int team, const Vector2& pos) {
    rallyPoints[team] = pos;
}

void BattleController::SetSquadObjective(Squad* squad, const Vector2& pos) {
    if (squad) {
        squadObjectives[squad] = pos;
    }
}

void BattleController::ClearSquadObjective(Squad* squad) {
    squadObjectives.erase(squad);
}

int BattleController::TotalMembers(int team) const {
    int total = 0;
    for (const auto& squad : squads) {
        if (squad->GetTeam() == team) {
            total += squad->GetMembers();
        }
    }
    return total;
}

void BattleController::SetCommandPoints(int team, int points) {
    commandPoints[team] = points;
}

int BattleController::GetCommandPoints(int team) const {
    auto it = commandPoints.find(team);
    return it != commandPoints.end() ? it->second : 0;
}

FlowField* BattleController::GetTeamField(int team) {
    auto it = teamFields.find(team);
    if (it != teamFields.end()) {
        return it->second.get();
    }
    // 惰性建立：以地形欄位為底，計算該隊目標的 flow field
    auto objIt = objectives.find(team);
    if (objIt == objectives.end()) {
        return &field;
    }
    auto teamField = std::make_unique<FlowField>(field);
    teamField->Compute(objIt->second);
    FlowField* ptr = teamField.get();
    teamFields[team] = std::move(teamField);
    return ptr;
}

bool BattleController::BeginExecution() {
    if (phase != BattlePhase::Deployment) {
        return false;
    }
    // 每隊目標各自一張 flow field
    for (const auto& kv : objectives) {
        GetTeamField(kv.first);
    }
    phase = BattlePhase::Execution;
    Emit("=== Execution phase begins ===");
    return true;
}

void BattleController::SetTimeScale(float scale) {
    timeScale = std::max(0.0f, scale);
}

bool BattleController::Intervene(Squad* squad, SquadOrder order,
                                 const Vector2& target, float holdSeconds) {
    if (phase != BattlePhase::Execution || !squad || squad->IsEliminated() ||
        squad->IsRouting()) {
        return false;
    }
    int team = squad->GetTeam();
    if (GetCommandPoints(team) <= 0) {
        Emit("Intervene denied: no command points");
        return false;
    }
    commandPoints[team]--;
    squad->IssueOrder(order, target);
    interventionUntil[squad] = holdSeconds;
    Emit("CP intervention on " + squad->GetName());
    return true;
}

bool BattleController::Intervene(Squad* squad, SquadOrder order,
                                 const Squad* target, float holdSeconds) {
    // 非 Engage 的 squad 目標指令退化為位置指令（engageTarget 只對 Engage 有意義）
    if (order != SquadOrder::Engage && target) {
        return Intervene(squad, order, target->GetPosition(), holdSeconds);
    }
    // 允許 Engage 潰逃中的敵隊——追殺是合法指令，損員由
    // DetectGovernanceEvents 記暴行入帳
    if (!target || target->IsEliminated() ||
        target == squad || (squad && target->GetTeam() == squad->GetTeam()) ||
        IsHiddenByFog(*target)) {
        return false;
    }
    if (phase != BattlePhase::Execution || !squad || squad->IsEliminated() ||
        squad->IsRouting()) {
        return false;
    }
    int team = squad->GetTeam();
    if (GetCommandPoints(team) <= 0) {
        Emit("Intervene denied: no command points");
        return false;
    }
    commandPoints[team]--;
    squad->IssueOrder(order, target);
    interventionUntil[squad] = holdSeconds;
    Emit("CP intervention on " + squad->GetName());
    return true;
}

bool BattleController::GeneralRally(Squad* general, float radius,
                                    float moraleBoost) {
    if (phase != BattlePhase::Execution || !general ||
        !general->IsGeneralGuard() ||
        general->IsEliminated() || general->IsRouting()) {
        return false;
    }
    int team = general->GetTeam();
    if (GetCommandPoints(team) <= 0) {
        Emit("General rally denied: no command points");
        return false;
    }
    commandPoints[team]--;
    int affected = 0;
    for (auto& squad : squads) {
        if (squad->GetTeam() != team || squad->IsEliminated() ||
            squad->IsRouting()) {
            continue;
        }
        if ((squad->GetPosition() - general->GetPosition()).Length()
                <= radius) {
            squad->AdjustMorale(moraleBoost);
            ++affected;
        }
    }
    Emit(general->GetName() + " rally shout (" +
         std::to_string(affected) + " squads heartened)");
    return true;
}

bool BattleController::GeneralCharge(Squad* general, float seconds) {
    if (phase != BattlePhase::Execution || !general ||
        !general->IsGeneralGuard() ||
        general->IsEliminated() || general->IsRouting()) {
        return false;
    }
    int team = general->GetTeam();
    if (GetCommandPoints(team) <= 0) {
        Emit("General charge denied: no command points");
        return false;
    }
    commandPoints[team]--;
    general->StartCharge(seconds);
    Emit(general->GetName() + " leads the charge!");
    return true;
}

float BattleController::CounterMultiplier(UnitClass atk, UnitClass def) {
    // 騎>弓、弓>步、步>騎；克制 ×1.5、被克 ×0.7、同種/無關 ×1.0
    using UC = UnitClass;
    if ((atk == UC::Cavalry && def == UC::Archer) ||
        (atk == UC::Archer && def == UC::Infantry) ||
        (atk == UC::Infantry && def == UC::Cavalry)) {
        return 1.5f;
    }
    if ((def == UC::Cavalry && atk == UC::Archer) ||
        (def == UC::Archer && atk == UC::Infantry) ||
        (def == UC::Infantry && atk == UC::Cavalry)) {
        return 0.7f;
    }
    return 1.0f;
}

void BattleController::SetRoutShock(float radius, float moraleHit) {
    routShockRadius = (radius > 0.0f) ? radius : 0.0f;
    routShockMorale = moraleHit;
}

void BattleController::ApplyRoutShock() {
    if (routShockRadius <= 0.0f || routShockMorale <= 0.0f) return;
    for (const auto& r : squads) {
        // 每支潰逃隊只擴散一次（IsRouting 終態，不會重複進入）
        if (!r->IsRouting() || routEmitted.count(r.get())) continue;
        routEmitted.insert(r.get());
        Emit(r->GetName() + " 潰逃！恐慌向友軍擴散");
        for (const auto& o : squads) {
            if (o.get() == r.get() || o->GetTeam() != r->GetTeam() ||
                o->IsEliminated() || o->IsRouting()) {
                continue;
            }
            const float dist =
                (o->GetPosition() - r->GetPosition()).Length();
            if (dist <= routShockRadius) {
                o->AdjustMorale(-routShockMorale);
                // 被擊潰者下個 tick 成為新的震源 → 連鎖
                Emit(o->GetName() + " 目睹友軍潰逃，士氣動搖");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// P-3 擁擠阻塞（jamming）
// ---------------------------------------------------------------------------

void BattleController::SetJamming(float radius,
                                  const Quasi::JammingParams& params) {
    jamRadius = radius;
    jamParams = params;
    if (jamRadius <= 0.0f) {
        for (auto& s : squads) s->SetCrowdFactor(1.0f); // 關閉時復位
    }
}

void BattleController::ApplyJamming() {
    if (jamRadius <= 0.0f) return;
    // 員額加權密度：活著的小隊都佔地（敵我一律，擁擠不分陣營）
    std::vector<Vector2> pos;
    std::vector<float> weight;
    pos.reserve(squads.size());
    weight.reserve(squads.size());
    for (const auto& s : squads) {
        if (s->IsEliminated()) continue;
        pos.push_back(s->GetPosition());
        weight.push_back(static_cast<float>(s->GetMembers()));
    }
    for (const auto& s : squads) {
        if (s->IsEliminated()) {
            s->SetCrowdFactor(1.0f);
            continue;
        }
        const float d = Quasi::CrowdDensity(s->GetPosition(), pos, weight,
                                            jamRadius, jamParams.unitArea);
        s->SetCrowdFactor(Quasi::JammingSpeedFactor(d, jamParams));
    }
}

// ---------------------------------------------------------------------------
// C-2 治理事件
// ---------------------------------------------------------------------------

void BattleController::RecordGovernanceEvent(GovernanceEvent ev) {
    ++govEvents[ev];
    Emit(std::string("治理：") + GovernanceEventName(ev));
}

void BattleController::DetectGovernanceEvents() {
    for (const auto& s : squads) {
        // 只評敵軍（team 1）——第三方神話隊（team>=2）潰逃/被補刀
        // 不進治理帳：鬼軍不是「投降的敵軍」，記受降/暴行是幻影入帳
        if (s->GetTeam() != 1) continue;
        auto it = govRoutMembers.find(s.get());
        if (it == govRoutMembers.end()) {
            if (!s->IsRouting()) continue;
            // 新潰逃敵隊：受降（任其生還不追殺）+ 記錄潰逃時員額
            govRoutMembers[s.get()] = s->GetMembers();
            RecordGovernanceEvent(GovernanceEvent::SurrenderAccepted);
        } else {
            // 已記帳的潰逃隊：損員即暴行——含全滅（ApplyCasualties
            // 全滅時清 routing 旗標，故不能用 IsRouting 當守門）
            if (s->GetMembers() < it->second &&
                !govAtrocityDone.count(s.get())) {
                govAtrocityDone.insert(s.get());
                RecordGovernanceEvent(GovernanceEvent::Atrocity);
            }
            it->second = s->GetMembers();
        }
    }
}

void BattleController::SetMoraleExecution(float threshold, float rate) {
    moraleExecThreshold = threshold;
    moraleExecRate = rate;
}

void BattleController::Emit(const std::string& msg) {
    if (onEvent) {
        onEvent(msg);
    }
}

void BattleController::Update(float realDt) {
    if (phase != BattlePhase::Execution || outcome != BattleOutcome::Ongoing) {
        return;
    }
    float dt = realDt * timeScale;
    if (dt <= 0.0f) {
        return;
    }
    elapsed += dt;

    // 介入覆寫倒數
    for (auto& kv : interventionUntil) {
        kv.second -= dt;
    }

    UpdateContexts();

    doctrineTimer += dt;
    if (doctrineTimer >= DOCTRINE_INTERVAL) {
        doctrineTimer = 0.0f;
        EvaluateDoctrines();
    }

    ApplyJamming(); // P-3：移動前先按最新密度更新擁擠倍率

    for (auto& squad : squads) {
        // 潰逃小隊可能留著陳舊 orderTarget（如敵方 objective），
        // doctrine 又跳過潰逃隊——每 tick 重指集結點，確保往己方撤退
        // （同時修正 ApplyCasualties 潰逃路徑留下的舊目標）
        if (squad->IsRouting()) {
            auto rIt = rallyPoints.find(squad->GetTeam());
            if (rIt != rallyPoints.end()) {
                squad->IssueOrder(SquadOrder::Retreat, rIt->second);
            }
        }
        squad->Update(dt, GetTeamField(squad->GetTeam()));
    }

    ResolveCombat(dt);
    ApplyRoutShock(); // G-1：傷亡結算後擴散潰逃衝擊
    DetectGovernanceEvents(); // C-2：潰逃/暴行治理事件偵測

    // Q-1 敵情霧：情報時效/退相干推進 + 接觸偵查（正負面觀測）
    if (fog) {
        fog->Update(dt);
        // Emit 會跑使用者 callback,蒐集起來離開 fogEntities 迭代再發,
        // 避免 callback 裡 BindFogSquad/CreateSquad 造成 map 重雜湊
        std::vector<const Squad*> spotted;
        for (const auto& kv : fogEntities) {
            Squad* target = kv.first;
            const int entityId = kv.second;
            if (!target || target->IsEliminated()) continue;
            const UncertainEntity* ent = fog->GetEntity(entityId);
            if (!ent) continue;

            // 正面：敵對方小隊進入真身接觸範圍 → 揭露(已揭露則刷新時效)
            // 潰逃中的小隊失控逃亡,不算有效觀察者
            bool inContact = false;
            for (const auto& other : squads) {
                if (other->GetTeam() == target->GetTeam() ||
                    other->IsEliminated() || other->IsRouting()) {
                    continue;
                }
                if ((other->GetPosition() - target->GetPosition()).Length() <=
                    fogRevealRange) {
                    inContact = true;
                    break;
                }
            }
            if (inContact) {
                const bool was = fog->IsRevealed(entityId);
                if (fog->Reveal(entityId, target->GetPosition()) && !was) {
                    spotted.push_back(target);
                }
                continue;
            }

            if (!fog->IsRevealed(entityId)) {
                // 情報過期回疊加：還在 Engage 隱形目標的小隊失去目標,
                // 退回駐守,不再追著看不見的敵人跑
                for (const auto& other : squads) {
                    if (other->GetTeam() == target->GetTeam() ||
                        other->IsEliminated() || other->IsRouting()) {
                        continue;
                    }
                    if (other->GetOrder() == SquadOrder::Engage &&
                        other->GetEngageTarget() == target) {
                        other->IssueOrder(SquadOrder::Hold,
                                          other->GetPosition());
                    }
                }

                // 負面：目視覆蓋候選格但真身不在那 → 消去該候選(雲縮小,
                // Scout 才不會卡在同一格 modal 上空轉)
                for (size_t ci = 0; ci < ent->candidates.size(); ++ci) {
                    // 候選格就是真身所在 → 目視到的是「有人」而非「沒人」
                    if ((ent->candidates[ci] - target->GetPosition())
                            .Length() <= fogRevealRange) {
                        continue;
                    }
                    for (const auto& other : squads) {
                        if (other->GetTeam() == target->GetTeam() ||
                            other->IsEliminated() || other->IsRouting()) {
                            continue;
                        }
                        if ((other->GetPosition() - ent->candidates[ci])
                                .Length() <= fogRevealRange) {
                            fog->EliminateCandidate(entityId,
                                                    static_cast<int>(ci));
                            break;
                        }
                    }
                }
            }
        }
        for (const Squad* s : spotted) {
            Emit(s->GetName() + " 被我軍目擊（接觸偵查）");
        }
    }
    // 計畫加成依最新情報重評（揭露/消去/過期都會拉動 certainty）；
    // 放在 if(fog) 外——fog 中途解綁也要讓加成回滿（certainty 1）。
    // 冪等——情報沒變時 damagePerMember 逐位元不動
    if (plan) {
        plan->UpdateUncertainty(*this);
    }

    CheckOutcome();
}

SquadContext BattleController::BuildContext(const Squad& squad) const {
    auto it = contexts.find(const_cast<Squad*>(&squad));
    if (it != contexts.end()) {
        return it->second;
    }
    SquadContext ctx;
    ctx.self = &squad;
    ctx.now = elapsed;
    ctx.healthPct = squad.GetHealthPct();
    ctx.moralePct = squad.GetMorale();
    ctx.underAttack = squad.IsUnderAttack();
    return ctx;
}

bool BattleController::IsHiddenByFog(const Squad& squad) const {
    if (!fog) return false;
    auto it = fogEntities.find(const_cast<Squad*>(&squad));
    return it != fogEntities.end() && !fog->IsRevealed(it->second);
}

Squad* BattleController::FindNearestEnemy(const Squad& squad, float maxDist) const {
    Squad* best = nullptr;
    float bestDist = maxDist;
    for (const auto& other : squads) {
        if (other->GetTeam() == squad.GetTeam() || other->IsEliminated() ||
            other->IsRouting() || IsHiddenByFog(*other)) {
            continue;
        }
        float d = (other->GetPosition() - squad.GetPosition()).Length();
        if (d < bestDist) {
            bestDist = d;
            best = other.get();
        }
    }
    return best;
}

Squad* BattleController::FindWeakestEnemy(const Squad& squad, float maxDist) const {
    Squad* best = nullptr;
    int lowest = 0x7fffffff;
    for (const auto& other : squads) {
        if (other->GetTeam() == squad.GetTeam() || other->IsEliminated() ||
            other->IsRouting() || IsHiddenByFog(*other)) {
            continue;
        }
        float d = (other->GetPosition() - squad.GetPosition()).Length();
        if (d <= maxDist && other->GetMembers() < lowest) {
            lowest = other->GetMembers();
            best = other.get();
        }
    }
    return best;
}

Squad* BattleController::FindNearestEngagedAlly(const Squad& squad) const {
    Squad* best = nullptr;
    float bestDist = 1e30f;
    for (const auto& other : squads) {
        if (other->GetTeam() != squad.GetTeam() || other.get() == &squad ||
            other->IsEliminated() || !other->IsEngaged()) {
            continue;
        }
        float d = (other->GetPosition() - squad.GetPosition()).Length();
        if (d < bestDist) {
            bestDist = d;
            best = other.get();
        }
    }
    return best;
}

void BattleController::BindFog(QuantumFog* f) {
    // 舊 fog 若持有指向本 controller 的事件回調,先清掉再換綁,
    // 避免解綁後 fog 透過懸空 this 回呼
    if (fog && fog != f) {
        fog->SetEventCallback(nullptr);
    }
    fog = f;
    // Q-6：fog 態變化（觀測/揭露/消去/過期/糾纏/探測）併入戰報流,
    // BattleRecorder 因此能錄下機率雲變化供回放時間軸重現
    if (fog) {
        fog->SetEventCallback(
            [this](const std::string& msg) { Emit(msg); });
    }
    // (重)綁定時清掉超出新 fog 範圍的舊綁定——失效 entityId
    // 會讓小隊永久隱形又沒有機率雲;解綁(nullptr)則全清
    for (auto it = fogEntities.begin(); it != fogEntities.end();) {
        const bool stale = !fog || it->second < 0 ||
            it->second >= static_cast<int>(fog->EntityCount());
        if (stale) {
            it = fogEntities.erase(it);
        } else {
            ++it;
        }
    }
}

void BattleController::BindFogSquad(Squad* squad, int entityId) {
    if (!squad) return;
    // fog 已綁時驗證 id,無效綁定會讓小隊永遠隱形又沒有雲
    if (fog && (entityId < 0 ||
                entityId >= static_cast<int>(fog->EntityCount()))) {
        return;
    }
    // 一個 entity 只能對應一支小隊;重複綁定會讓 GetFogSquad 結果不固定
    for (const auto& kv : fogEntities) {
        if (kv.second == entityId && kv.first != squad) {
            return;
        }
    }
    fogEntities[squad] = entityId;
}

int BattleController::GetFogEntityId(const Squad* squad) const {
    auto it = fogEntities.find(const_cast<Squad*>(squad));
    return it != fogEntities.end() ? it->second : -1;
}

Squad* BattleController::GetFogSquad(int entityId) const {
    for (const auto& kv : fogEntities) {
        if (kv.second == entityId) return kv.first;
    }
    return nullptr;
}

bool BattleController::FindScoutTarget(const Squad& squad,
                                       Vector2& out) const {
    if (!fog) return false;
    float bestDist = 1e30f;
    bool found = false;
    for (const auto& kv : fogEntities) {
        const Squad* target = kv.first;
        const int entityId = kv.second;
        // 只偵查敵對方、未全滅、未揭露的實體
        if (!target || target->GetTeam() == squad.GetTeam() ||
            target->IsEliminated() || fog->IsRevealed(entityId)) {
            continue;
        }
        const auto cloud = fog->GetCloud(entityId);
        if (cloud.empty()) continue;
        // modal 候選格 = 機率最大者
        const Vector2 modal =
            std::max_element(cloud.begin(), cloud.end(),
                             [](const auto& a, const auto& b) {
                                 return a.second < b.second;
                             })->first;
        const float d = (modal - squad.GetPosition()).Length();
        if (d < bestDist) {
            bestDist = d;
            out = modal;
            found = true;
        }
    }
    return found;
}

void BattleController::UpdateContexts() {
    for (auto& squad : squads) {
        SquadContext ctx;
        ctx.self = squad.get();
        ctx.now = elapsed;
        ctx.healthPct = squad->GetHealthPct();
        ctx.moralePct = squad->GetMorale();
        ctx.underAttack = squad->IsUnderAttack();

        Squad* nearest = FindNearestEnemy(*squad, 1e30f);
        ctx.nearestEnemyDist = nearest
            ? (nearest->GetPosition() - squad->GetPosition()).Length()
            : -1.0f;

        // 敵眾我寡：交戰範圍內敵方成員總數 > 我方
        int enemyMembers = 0;
        for (const auto& other : squads) {
            if (other->GetTeam() != squad->GetTeam() && !other->IsEliminated() &&
                !IsHiddenByFog(*other) &&
                (other->GetPosition() - squad->GetPosition()).Length() <=
                    squad->GetEngageRange() * 3.0f) {
                enemyMembers += other->GetMembers();
            }
        }
        ctx.outnumbered = enemyMembers > squad->GetMembers();

        ctx.anyAllyEngaged = false;
        for (const auto& other : squads) {
            if (other->GetTeam() == squad->GetTeam() &&
                other.get() != squad.get() && other->IsEngaged()) {
                ctx.anyAllyEngaged = true;
                break;
            }
        }

        // G-5：每小隊箭頭目標優先於 team objective pin
        auto soIt = squadObjectives.find(squad.get());
        auto objIt = objectives.find(squad->GetTeam());
        const Vector2* objPos = soIt != squadObjectives.end()
                                    ? &soIt->second
                                    : (objIt != objectives.end() ? &objIt->second
                                                                 : nullptr);
        ctx.objectiveReached =
            objPos &&
            (*objPos - squad->GetPosition()).Length() <= OBJECTIVE_RADIUS;

        contexts[squad.get()] = ctx;
    }
}

void BattleController::EvaluateDoctrines() {
    for (auto& squad : squads) {
        if (squad->IsEliminated() || squad->IsRouting()) {
            continue;
        }
        // 玩家 CP 介入覆寫中，跳過 doctrine
        auto iv = interventionUntil.find(squad.get());
        if (iv != interventionUntil.end() && iv->second > 0.0f) {
            continue;
        }

        auto docIt = doctrines.find(squad.get());
        if (docIt == doctrines.end()) {
            continue;
        }
        const SquadContext& ctx = contexts[squad.get()];
        DoctrineAction action = docIt->second.Evaluate(ctx);
        const DoctrineRule* rule = docIt->second.GetMatchedRule();
        if (rule) {
            // N-3：記錄各隊 trigger 命中分佈（對手軍師讀檔用）
            ++triggerUsage[squad->GetTeam()][rule->trigger];
        }

        // T-6 士氣執行率：士氣崩到門檻以下，命令有機率被抗命（原地不動）
        if (moraleExecThreshold > 0.0f &&
            ctx.moralePct < moraleExecThreshold &&
            action != DoctrineAction::RetreatToRally) { // 撤退永遠執行
            std::uniform_real_distribution<float> roll(0.0f, 1.0f);
            if (roll(execRng) > moraleExecRate) {
                Emit(squad->GetName() + " ignored orders (low morale)");
                continue;
            }
        }

        SquadOrder prevOrder = squad->GetOrder();
        Vector2 prevTarget = squad->GetOrderTarget();
        const Squad* prevEngage = squad->GetEngageTarget();

        switch (action) {
        case DoctrineAction::AttackNearest: {
            Squad* target = FindNearestEnemy(*squad, 1e30f);
            if (target) {
                squad->IssueOrder(SquadOrder::Engage, target);
            }
            break;
        }
        case DoctrineAction::AttackWeakest: {
            Squad* target = FindWeakestEnemy(*squad, 1e30f);
            if (target) {
                squad->IssueOrder(SquadOrder::Engage, target);
            }
            break;
        }
        case DoctrineAction::AdvanceToObjective: {
            auto soIt = squadObjectives.find(squad.get());
            auto objIt = objectives.find(squad->GetTeam());
            const Vector2* objPos = soIt != squadObjectives.end()
                                        ? &soIt->second
                                        : (objIt != objectives.end()
                                               ? &objIt->second
                                               : nullptr);
            if (objPos) {
                GetTeamField(squad->GetTeam()); // 確保場已算好
                squad->IssueOrder(SquadOrder::AttackMove, *objPos);
            }
            break;
        }
        case DoctrineAction::HoldPosition:
            squad->IssueOrder(SquadOrder::Hold, squad->GetPosition());
            break;
        case DoctrineAction::RetreatToRally: {
            auto rIt = rallyPoints.find(squad->GetTeam());
            Vector2 dest = rIt != rallyPoints.end() ? rIt->second
                                                  : squad->GetPosition();
            squad->IssueOrder(SquadOrder::Retreat, dest);
            break;
        }
        case DoctrineAction::DefendNearestAlly: {
            Squad* ally = FindNearestEngagedAlly(*squad);
            if (ally) {
                squad->IssueOrder(SquadOrder::MoveTo, ally->GetPosition());
            }
            break;
        }
        case DoctrineAction::Scout: {
            Vector2 dest;
            if (FindScoutTarget(*squad, dest)) {
                squad->IssueOrder(SquadOrder::AttackMove, dest);
            } else {
                // 沒有未揭露的敵情雲——退回駐守
                squad->IssueOrder(SquadOrder::Hold, squad->GetPosition());
            }
            break;
        }
        }

        bool orderChanged =
            squad->GetOrder() != prevOrder ||
            squad->GetEngageTarget() != prevEngage ||
            (squad->GetOrderTarget() - prevTarget).LengthSquared() > 0.01f;

        if (rule && orderChanged && rule->trigger != DoctrineTrigger::Always) {
            std::ostringstream oss;
            oss << squad->GetName() << " doctrine: " << TriggerName(rule->trigger)
                << " -> " << ActionName(rule->action);
            Emit(oss.str());
        }
    }
}

void BattleController::ResolveCombat(float dt) {
    // 每 tick 重置戰鬥旗標，接戰判定再立起來
    for (auto& squad : squads) {
        squad->SetEngaged(false);
        squad->SetUnderAttack(false);
    }

    // G-2：每防禦方本 tick 已輸出的攻擊者數（combat width 用）
    std::unordered_map<Squad*, int> attackersOn;

    // G-4 地形修正：高地攻方 ×1.25、森林守方 ×0.7（泥濘走 cost）
    auto terrainFactor = [this](const Squad* atk, const Squad* def) {
        float f = 1.0f;
        if (field.TerrainAt(atk->GetPosition()) == TerrainType::Highland) {
            f *= 1.25f;
        }
        if (field.TerrainAt(def->GetPosition()) == TerrainType::Forest) {
            f *= 0.7f;
        }
        return f;
    };

    for (size_t i = 0; i < squads.size(); ++i) {
        Squad* a = squads[i].get();
        if (a->IsEliminated() || a->IsRouting()) {
            continue;
        }
        for (size_t j = i + 1; j < squads.size(); ++j) {
            Squad* b = squads[j].get();
            if (b->IsEliminated() || a->GetTeam() == b->GetTeam()) {
                continue;
            }
            // 潰逃者不會反擊（GetAttackDPS=0），也不再被鄰近自動
            // 波及——只有被明確 Engage 點名追殺時才吃單向傷害，
            // 讓「任其生還/追殺已降」成為可歸責的指令行為
            // （DetectGovernanceEvents 據損員記暴行）
            if (b->IsRouting()) {
                if (a->GetOrder() == SquadOrder::Engage &&
                    a->GetEngageTarget() == b &&
                    (a->GetPosition() - b->GetPosition()).Length() <=
                        a->GetEngageRange() &&
                    (!fog || !IsHiddenByFog(*b))) {
                    if (combatWidth <= 0 ||
                        attackersOn[b] < combatWidth) {
                        damageBuffer[b] +=
                            a->GetAttackDPS() *
                            CounterMultiplier(a->GetUnitClass(),
                                              b->GetUnitClass()) *
                            terrainFactor(a, b) * dt;
                        ++attackersOn[b];
                    }
                    b->SetUnderAttack(true);
                }
                continue;
            }
            // 霧中未揭露的小隊對敵方不可見——engageRange 大於
            // fogRevealRange 時也不能隔空交戰(無 fog 時不付查表成本)
            if (fog && (IsHiddenByFog(*a) || IsHiddenByFog(*b))) {
                continue;
            }
            float dist = (a->GetPosition() - b->GetPosition()).Length();
            float range = std::max(a->GetEngageRange(), b->GetEngageRange());
            if (dist <= range) {
                a->SetEngaged(true);
                b->SetEngaged(true);
                a->SetUnderAttack(true);
                b->SetUnderAttack(true);
                // G-2 克制三角 × 戰線寬度：超過 width 的攻擊者
                // 維持接戰（圍觀）但不輸出
                if (combatWidth <= 0 || attackersOn[b] < combatWidth) {
                    damageBuffer[b] += a->GetAttackDPS() *
                        CounterMultiplier(a->GetUnitClass(),
                                          b->GetUnitClass()) *
                        terrainFactor(a, b) * dt;
                    ++attackersOn[b];
                }
                if (combatWidth <= 0 || attackersOn[a] < combatWidth) {
                    damageBuffer[a] += b->GetAttackDPS() *
                        CounterMultiplier(b->GetUnitClass(),
                                          a->GetUnitClass()) *
                        terrainFactor(b, a) * dt;
                    ++attackersOn[a];
                }
            }
        }
    }

    // 結算傷亡與士氣回復
    for (auto& squad : squads) {
        auto it = damageBuffer.find(squad.get());
        if (it != damageBuffer.end()) {
            int casualties = static_cast<int>(it->second);
            if (casualties > 0) {
                int before = squad->GetMembers();
                squad->ApplyCasualties(casualties);
                it->second -= static_cast<float>(casualties);
                Emit(squad->GetName() + " took " +
                     std::to_string(before - squad->GetMembers()) +
                     " casualties");
                if (squad->IsEliminated()) {
                    Emit(squad->GetName() + " ELIMINATED");
                } else if (squad->IsRouting()) {
                    Emit(squad->GetName() + " is routing!");
                }
            }
        }
        if (!squad->IsEliminated()) {
            squad->RecoverMorale(dt);
        }
    }
}

void BattleController::CheckOutcome() {
    bool alive[2] = {false, false};
    bool generalSlain[2] = {false, false};
    for (const auto& squad : squads) {
        // 第三方隊（team>=2,如神話入侵鬼軍）不計勝負——
        // 忽視存活的鬼軍不該讓戰鬥永遠 Ongoing
        const int team = squad->GetTeam();
        if (team != 0 && team != 1) continue;
        int t = team;
        if (!squad->IsEliminated() && !squad->IsRouting()) {
            alive[t] = true;
        }
        // G-8：將軍衛隊全滅即敗；潰逃不算陣亡，將軍可被擊退再回來
        if (squad->IsGeneralGuard() && squad->IsEliminated()) {
            generalSlain[t] = true;
        }
    }
    // 衛隊陣亡優先於一般消滅判定（雙方衛隊同歿 → 平手）
    if (generalSlain[0] || generalSlain[1]) {
        Emit("general slain!");
        if (generalSlain[0] && generalSlain[1]) {
            outcome = BattleOutcome::Draw;
        } else if (generalSlain[0]) {
            outcome = BattleOutcome::Defeat;
        } else {
            outcome = BattleOutcome::Victory;
        }
    } else if (!alive[0] && !alive[1]) {
        outcome = BattleOutcome::Draw;
    } else if (!alive[1]) {
        outcome = BattleOutcome::Victory;
    } else if (!alive[0]) {
        outcome = BattleOutcome::Defeat;
    }

    if (outcome != BattleOutcome::Ongoing) {
        phase = BattlePhase::Resolution;
        Emit("=== Resolution: outcome " +
             std::to_string(static_cast<int>(outcome)) + " ===");
    }
}

} // namespace Gameplay
} // namespace Potato
