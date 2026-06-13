# Bot Commands

This document describes all available commands of the bot.

## Administrator commands

### `/set-settings`

Set the needed settings for the bot to work.


| Parameter       | Type      | Required | Description                                            |
| --------------- | --------- | -------- | ------------------------------------------------------ |
| `channel`       | `channel` | ✅       | Set the moderator channel to receive players' submits. |
| `grounddweller` | `role`    | ✅       | Set the Ground Dweller role to use.                    |
| `beginner`      | `role`    | ✅       | Set the Beginner role to use.                          |
| `apprentice`    | `role`    | ✅       | Set the Apprentice role to use.                        |
| `intermediate`  | `role`    | ✅       | Set the Intermediate role to use.                      |
| `advanced`      | `role`    | ✅       | Set the Advanced role to use.                          |
| `expert`        | `role`    | ✅       | Set the Expert role to use.                            |
| `master`        | `role`    | ✅       | Set the Master role to use.                            |
| `legend`        | `role`    | ✅       | Set the Legend role to use.                            |
| `mythic`        | `role`    | ✅       | Set the Mythic role to use.                            |
| `demigod`       | `role`    | ✅       | Set the Demigod role to use.                           |
| `airdribblegod` | `role`    | ✅       | Set the Air Dribble God role to use.                   |

#### Functionality

- `channel` parameter
  - Save the channel ID got from the parameter.
  - It has to be saved in the `channel.json` file with the name of `channelID`.
- Parameters to set the roles to use
  - All parameters' role ID has to be saved in the `ranks.json` file.
  - Each ID has to be saved as `roleID` in their respective ranks. Example:

    ```json
    {
        "grounddweller": {
            "name": "Ground Dweller",
            "ratingNeeded": 0,
            "roleID": "1486747106063683604",
            "players": []
        },
        "beginner": {
            "name": "Beginner",
            "ratingNeeded": 110,
            "roleID": "1278281473354371134",
            "players": []
        },
        ...
    }
    ```

    **Note on `ranks.json` structure:**
    Every rank contains a `players` array with objects `{ "id": "player_id" }`, **except** the `airdribblegod` rank, which stores a single `player` field (a Discord ID string) and a `ratingNeeded` that is set to `0` when the title is vacant.

#### Behavior notes

- **If the bot's role is lower than the roles configured in the command** → replies with an ephemeral error message asking to move the bot's role above all the rank roles, since it needs a higher role to assign or remove them from players.
- **If `channel` is set to a non-text channel** → replies with an ephemeral message and asks for a valid text channel.

---

### `/challenge-create`

Create a new challenge.

#### Subcommands

##### `normal`

Create a normal challenge.


| Parameter     | Type     | Required | Description                        |
| ------------- | -------- | -------- | ---------------------------------- |
| `name`        | `string` | ✅       | Challenge's name.                  |
| `rank`        | `string` | ✅       | Challenge's rank (string options). |
| `rating`      | `number` | ✅       | Rating value.                      |
| `description` | `string` | ✅       | Description.                       |
| `url1`        | `string` | ✅       | First example's url.               |
| `url2`        | `string` | ❌       | Optional second example's url.     |

##### `bonus`

Create a bonus challenge.


| Parameter     | Type     | Required | Description                    |
| ------------- | -------- | -------- | ------------------------------ |
| `name`        | `string` | ✅       | Challenge's name.              |
| `rating`      | `number` | ✅       | Rating value.                  |
| `description` | `string` | ✅       | Description.                   |
| `url1`        | `string` | ✅       | First example's url.           |
| `url2`        | `string` | ❌       | Optional second example's url. |

#### Numbering scheme

- **Normal challenges**:
  The `number` is composed of a **rank prefix** followed by a sequential index within that rank.


  | Rank            | Number range |
  | --------------- | ------------ |
  | Beginner        | 1, 2, 3, …  |
  | Apprentice      | 101, 102, … |
  | Intermediate    | 201, 202, … |
  | Advanced        | 301, 302, … |
  | Expert          | 401, 402, … |
  | Master          | 501, 502, … |
  | Legend          | 601, 602, … |
  | Mythic          | 701, 702, … |
  | Demigod         | 801, 802, … |
  | Air Dribble God | 901, 902, … |
- **Bonus challenges**:
  Sequential numbering starting at 1 (1, 2, 3, …).

#### Functionality

- `normal` subcommand
  - Save all the parameter values into the `challenges.json` file
  - Sort all the challenges first by rank from lower to higher (order from the `ranks.json` file) and then by rating from lower to higher. The first challenge in the `challenges` array should be the lower in rank and rating.
  - Temporarily store every player's completed normal challenge numbers and the numbers of the normal challenges waiting to be rated, because they need to be updated after re-sorting.
  - Reassign a new `number` to each challenge following the **normal numbering scheme** (rank‑prefixed), and update the stored numbers in players' profiles and pending ratings accordingly.
  - Recalculate every rank's `ratingNeeded` in `ranks.json`:
    - For a given rank, sum the ratings of all challenges **from the lowest rank up to that rank**.
    - Multiply the total by 1.1.
    - Store the result as the new `ratingNeeded` for that rank.
    - *Why?* This makes it harder for players to rank up by only completing high-rank challenges; they need to engage with lower-rank challenges as well.
  - Update each player's rating to prevent unfair deranking due to the new challenge:
    - After recalculating all `ratingNeeded` values, for every player whose rank is **equal to or higher than** the rank of the newly created challenge:
      1. Take the `ratingNeeded` of their current rank.
      2. Divide it by 1.1 to obtain the base rating that corresponds to all challenges up to their rank.
      3. Add the ratings of all challenges **above** their current rank.
      4. Set this total as the player's new rating.
    - *Why?* Adding a new challenge raises the `ratingNeeded` threshold for its rank and all higher ranks. Without this adjustment, a player could suddenly fall below the required rating for their rank through no fault of their own. This calculation effectively grants them the new challenge as if they had already completed it, preserving their current rank.
- `bonus` subcommand
  - Save all the parameter values into the `bonus.json` file
  - Sort all the bonus challenge by rating from lower to higher. The first challenge in the `challenges` array should be the lower in rating.
  - Temporarily store every player's completed bonus challenge numbers and the numbers of the bonus challenges waiting to be rated, because they need to be updated after re-sorting.
  - Reassign a new `number` to each challenge using the **bonus numbering scheme** (sequential), and update the stored numbers in players' profiles and pending ratings accordingly.
- `challenges.json` file's structure should be like this:
  ```json
  {
      "challenges": [
          {
              "challengeName": "Example challenge",
              "rank": "beginner",
              "number": 1,
              "rating": 15,
              "description": "This is an example challenge",
              "url1": "https://youtu.be/fBIQIDfUfr8?t=100",
              "url2": "https://youtu.be/a_GySzNlUSw"
          },
        ...
      ]
  }
  ```
- `bonus.json` file's structure should be like this:
  ```json
  {
      "challenges": [
          {
              "challengeName": "Bonus example challenge",
              "number": 1,
              "rating": 15,
              "description": "This is an example challenge",
              "url1": "https://youtu.be/fBIQIDfUfr8?t=100",
              "url2": "https://youtu.be/a_GySzNlUSw"
          },
        ...
      ]
  }
  ```
- Update Discord roles for every player whose rank changed (including the Air Dribble God title), removing old roles and assigning the new ones according to their updated rank in `ranks.json`.

#### Behavior notes

- **If the url2 is not given** → save it as `null`.
- **If two challenges share the same rating (and the same rank for normal ones)** → a stable sort keeps their relative insertion order (oldest first). Since new challenges are appended before sorting, no extra logic is needed.
- **If the rating of the Air Dribble God changes** → update the needed rating for Air Dribble God (the same rating as the actual Air Dribble God player).

---

### `/challenge-edit`

Edit an existing challenges.

#### **SubcommandGroup**

##### `normal`

Edit or delete a normal challenge.

##### **Subcommands**

##### `edit`

Edit a normal challenge.


| Parameter     | Type      | Required | Description                             |
| ------------- | --------- | -------- | --------------------------------------- |
| `number`      | `number`  | ✅       | Number of the challenge to edit.        |
| `name`        | `string`  | ❌       | Edit challenge's name.                  |
| `rank`        | `string`  | ❌       | Edit challenge's rank (string options). |
| `rating`      | `number`  | ❌       | Edit rating value.                      |
| `description` | `string`  | ❌       | Edit description.                       |
| `url1`        | `string`  | ❌       | Edit first example's url.               |
| `url2`        | `string`  | ❌       | Edit optional second example's url.     |
| `deleteUrl2`  | `boolean` | ❌       | Delete the second url.                  |

##### `delete`

Delete a normal challenge.


| Parameter | Type     | Required | Description                        |
| --------- | -------- | -------- | ---------------------------------- |
| `number`  | `number` | ✅       | Number of the challenge to delete. |

#### **SubcommandGroup**

##### `bonus`

Edit or delete a bonus challenge.

##### **Subcommands**

##### `edit`

Edit a normal challenge.


| Parameter     | Type      | Required | Description                         |
| ------------- | --------- | -------- | ----------------------------------- |
| `number`      | `number`  | ✅       | Number of the challenge to edit.    |
| `name`        | `string`  | ❌       | Edit challenge's name.              |
| `rating`      | `number`  | ❌       | Edit rating value.                  |
| `description` | `string`  | ❌       | Edit description.                   |
| `url1`        | `string`  | ❌       | Edit first example's url.           |
| `url2`        | `string`  | ❌       | Edit optional second example's url. |
| `deleteUrl2`  | `boolean` | ❌       | Delete the second url.              |

##### `delete`

Delete a normal challenge.


| Parameter | Type     | Required | Description                        |
| --------- | -------- | -------- | ---------------------------------- |
| `number`  | `number` | ✅       | Number of the challenge to delete. |

#### Numbering scheme

- **Normal challenges**:
  The `number` is composed of a **rank prefix** followed by a sequential index within that rank.


  | Rank            | Number range |
  | --------------- | ------------ |
  | Beginner        | 1, 2, 3, …  |
  | Apprentice      | 101, 102, … |
  | Intermediate    | 201, 202, … |
  | Advanced        | 301, 302, … |
  | Expert          | 401, 402, … |
  | Master          | 501, 502, … |
  | Legend          | 601, 602, … |
  | Mythic          | 701, 702, … |
  | Demigod         | 801, 802, … |
  | Air Dribble God | 901, 902, … |
- **Bonus challenges**:
  Sequential numbering starting at 1 (1, 2, 3, …).

#### Functionality

- `normal edit` subcommand
  - Find the challenge by its current `number` in `challenges.json`.
  - Update only the fields that were provided; leave the rest unchanged.
  - If `deleteUrl2` is `true`, set `url2` to `null`.
  - If `rank` or `rating` was changed → apply the same re-sort and re-number logic described in `/challenge-create`:
    1. Temporarily store every player's completed normal challenge numbers and the numbers of the challenges waiting to be rated.
    2. Sort the `challenges` array by rank (lowest to highest, according to `ranks.json`) and then by rating (lowest to highest).
    3. Reassign a new `number` to each challenge according to the **numbering scheme** (rank‑prefixed for normal, sequential for bonus).
    4. Update the stored numbers in players' profiles and pending ratings.
  - Update each player's normal rating. The rank **does not** change automatically — if a player's rating drops below the threshold for their current rank, they are **not** deranked.
  - Re-sort the players in `players.json` by normal rating from highest to lowest, so the player with the highest rating (the Air Dribble God) always appears first in the array.
  - Update Discord roles for all affected players (especially if the Air Dribble God changed or players moved between ranks).
  - If neither `rank` nor `rating` was changed → the order and numbering remain untouched.
- `normal delete` subcommand
  - Remove the challenge with the given `number` from `challenges.json`.
  - Re-sort and re-number the remaining challenges using the same logic (temporarily store numbers, sort, reassign, update players and pending ratings).
  - Update each player's normal rating. The rank **does not** change automatically — if a player's rating drops below the threshold for their current rank, they are **not** deranked.
  - Re-sort the players in `players.json` by normal rating from highest to lowest, so the player with the highest rating (the Air Dribble God) always appears first in the array.
  - Update Discord roles for all affected players (especially if the Air Dribble God changed or players moved between ranks).
- `bonus edit` subcommand
  - Find the challenge by its current `number` in `bonus.json`.
  - Update only the provided fields; leave the rest unchanged.
  - If `deleteUrl2` is `true`, set `url2` to `null`.
  - If `rating` was changed → re-sort by rating ascending, reassign numbers, and update stored numbers in players' profiles and pending ratings.
  - If `rating` was not changed → order and numbering remain untouched.
  - Update players' bonus rating.
- `bonus delete` subcommand
  - Remove the challenge with the given `number` from `bonus.json`.
  - Re-sort by rating ascending, reassign numbers, and update stored numbers in players' profiles and pending ratings.
  - Update players' bonus rating.

#### Behavior notes

- **If the provided `number` does not exist** → reply with an ephemeral error message: "Challenge #X not found."
- **If `edit` is used without any optional parameter (and `deleteUrl2` is not `true`)** → reply with an ephemeral message: "No changes were provided." and leave the challenge unchanged.
- **If `deleteUrl2` is `true` but `url2` is already `null`** → no change is made to `url2`; other provided fields are still updated normally.
- **If deleting a challenge causes the array to become empty** → the re-number logic simply produces an empty array; no updates to players are needed.
- **If the `bonus edit` or `bonus delete` was used** → no re-sort of `players.json`, the order of the players is from highest to lowest by normal challenges' rating.

---

### `/complete-challenge`

Mark a challenge as completed for a player.


| Parameter | Type      | Required | Description                                          |
| --------- | --------- | -------- | ---------------------------------------------------- |
| `player`  | `user`    | ✅       | The player whose challenge is going to be completed. |
| `bonus`   | `boolean` | ✅       | Choose between a normal or bonus challenge.          |
| `number`  | `number`  | ✅       | The number of the challenge.                         |
| `url`     | `string`  | ✅       | URL of the video.                                    |

#### Functionality

- Look up the challenge by its `number`:
  - If `bonus` is `false` → search in `challenges.json`.
  - If `bonus` is `true` → search in `bonus.json`.
- **If the challenge does not exist** → reply with an ephemeral error: "Challenge #X not found."
- **If the player has already completed that challenge** (same number and type) → reply with an ephemeral error: "Player has already completed this challenge."
- Otherwise, add the challenge to the player's completed list:
  - For normal challenges: append `{ "number": X, "url": "..." }` to the player's `completedNormalChallenges`.
  - For bonus challenges: append to `completedBonus`.
- Update the player's rating:
  - **Normal challenge**:

    1. Add the challenge's rating to the player's normal rating.
    2. After adding, check if the player qualifies for a higher rank:

       - Compare the new rating against the `ratingNeeded` of each rank (from lowest to highest).
       - If the player's rating is ≥ the `ratingNeeded` of a higher rank, promote them to that rank (update their `rank` field and assign the corresponding Discord role).
    3. If the player is promoted to a higher rank:

       - Recalculate their rating to include all challenges up to the new rank.

       1. Take the `ratingNeeded` of the **new** rank.
       2. Divide it by 1.1 (this gives the total rating of all challenges up to that rank).
       3. Add the ratings of all challenges **above** the new rank.
       4. Set this total as the player's updated normal rating.

       - After recalculating, repeat step `2` (the rank‑qualification check) because the recalculated rating might now qualify the player for an even higher rank.
  - **Bonus challenge**:

    - Add the challenge's rating to the player's bonus rating.
    - *(Bonus rating does not affect rank; ranks are tied to normal rating only.)*
- Re-sort the `players` array in `players.json`:
  - After a normal challenge → sort by normal rating descending (highest first).
  - After a bonus challenge → no re-sort is needed (player order depends on normal rating).
- After re‑sorting, update Discord roles for any player whose rank changed (including the Air Dribble God), removing old roles and assigning new ones.
- Reply with a confirmation message (e.g., "Challenge #X marked as completed for @player. New rating: Y").

#### Behavior notes

- **If the challenge number is not found** → ephemeral error: "Challenge #X not found."
- **If the player already completed this challenge** → ephemeral error: "Player has already completed this challenge."
- **If the player completes a challenge from a higher rank than their current one** → they still receive the rating and may rank up immediately if the new total crosses the threshold(s).
- **If the player's new rating qualifies them for multiple rank promotions at once** → they skip intermediate ranks and are promoted directly to the highest qualifying rank.
- **If the bot lacks permissions to assign the new role** → the rating and rank are updated in the data files, but the role assignment fails; reply with a warning that the role could not be assigned and ask an admin to check the bot's role hierarchy.
- **If the player is not yet present in `players.json`:**

  1. Add the player into players.json with a default object:

  ```json
  {
      "players": [
          ...,
          {
              "id": "player id",
              "rank": "grounddweller",
              "rating": 0,
              "bonusRating": 0,
              "completedChallenges": [],
              "completedBonus": [],
              "challengesWaitingToBeRated": [],
              "bonusWaitingToBeRated": []
          }
      ]
  }
  ```

  2. Then apply the completion logic described above (add the challenge, update rating, check promotions, etc.).

---

### `/derank`

Derank a player by one rank each time.


| Parameter | Type   | Required | Description                         |
| --------- | ------ | -------- | ----------------------------------- |
| `player`  | `user` | ✅       | The player to derank just one rank. |

#### Functionality

- Look up the player in `players.json` by their Discord ID.
- **If the player is not found** → reply with an ephemeral error: "Player not found in the ranking system."
- Determine the player's current rank.
  - If the player is already at the **lowest rank** (Ground Dweller) → reply with an ephemeral error: "Player is already at the lowest rank and cannot be deranked further."
- Check the **derank conditions**:
  1. **Has the player completed all normal challenges of their current rank?**
     - Count the total number of normal challenges belonging to that rank in `challenges.json`.
     - Compare with the player's `completedChallenges` that have numbers within that rank's range.
     - If the player has completed **every** challenge of their current rank → reply with an ephemeral error: "Player has completed all challenges of their current rank and cannot be manually deranked."
  2. **Is the player's rating below the `ratingNeeded` for their current rank?**
     - Compare the player's normal rating (from `players.json`) with the `ratingNeeded` value of their rank in `ranks.json`.
     - If the rating is **still equal to or above** the required value → reply with an ephemeral error: "Player's rating still meets the requirements for their current rank. Derank not allowed."
- If both conditions are satisfied:
  - Move the player down one rank:

    - **If the player is the current Air Dribble God:**
      - Find the player's underlying rank by searching all other rank `players` arrays.
      - Remove the player from that array.
      - Set `airdribblegod.player` to `null` and `airdribblegod.ratingNeeded` to `0` (a new Air Dribble God will be chosen later).
      - The target rank for deranking is the rank immediately below the underlying rank found.
    - **Otherwise:**
      - Remove the player from their current rank's `players` array.
      - Add the player's ID to the `players` array of the immediately lower rank.
      - If the new rank is Air Dribble God, set `airdribblegod.player` to the player's ID (instead of pushing to an array).
  - **Recalculate the player's normal rating** to match the new rank:

    1. Take the `ratingNeeded` of the **new** rank.
    2. Divide it by 1.1 (this gives the total rating of all challenges up to and including that rank).
    3. Add the ratings of any challenges from ranks **above** the new rank that the player has completed (higher-rank challenges keep their value).
    4. Set this sum as the player's new rating.

    - *Why?* This ensures the player receives credit for higher‑rank challenges they have already done, while treating all challenges up to their new rank as inherently completed (without needing to track which lower‑rank challenges they originally finished).
- Re‑sort the affected rank lists (and the global `players` array) by normal rating descending.
- Update Discord roles for the deranked player (remove old rank role, assign new rank role).
- If the deranked player was the Air Dribble God, also remove the Air Dribble God role.
- After the global recalculation, assign the Air Dribble God role to the new highest‑rated player (if any) and update `airdribblegod.player` accordingly.
- Reply with a confirmation message (e.g., "@player has been deranked to [New Rank]. New rating: X.").

#### Behavior notes

- **If the player is not in `players.json`** → ephemeral error: "Player not found."
- **If the player is Ground Dweller** → ephemeral error: "Already at the lowest rank."
- **If the player has completed all challenges of their current rank** → ephemeral error: "Cannot derank — all challenges of the current rank have been completed."
- **If the player's rating is still ≥ `ratingNeeded` of their current rank** → ephemeral error: "Derank not allowed — rating still meets the rank requirement."
- **If the bot lacks the `Manage Roles` permission** → the data files are updated, but the role change fails; reply with a warning to check the bot's role hierarchy.
- **Deranking does not affect bonus rating or bonus challenges**; only the normal rank and rating are modified.
- **Air Dribble God rank uses a single `player` field, not an array.**
  When a player becomes Air Dribble God, `airdribblegod.player` stores their ID; otherwise it is `null`. Deranking the current Air Dribble God clears this field and forces a new one to be selected after recalculation.

---

### `/completed-challenge-remove`

Delete a player's normal or bonus completed challenge


| Parameter | Type      | Required | Description                                           |
| --------- | --------- | -------- | ----------------------------------------------------- |
| `player`  | `user`    | ✅       | The player whose completed challenge will be removed. |
| `bonus`   | `boolean` | ✅       | Choose a bonus or normal challenge.                   |
| `number`  | `number`  | ✅       | Number of the challenge to delete.                    |

#### Functionality

- Look up the player in `players.json` by their Discord ID.
- **If the player is not found** → reply with an ephemeral error: "Player not found in the ranking system."
- Search for the challenge in the player's completed list:
  - If `bonus` is `false` → look in `completedChallenges`.
  - If `bonus` is `true` → look in `completedBonus`.
- **If the challenge number is not found** in that list → reply with an ephemeral error: "Player has not completed challenge #X." (or "Player has not completed bonus challenge #X.")
- Remove the matching entry from the player's completed list.
- **Recalculate the player's rating:**
  - **Normal challenge removed:**
    1. Recalculate the player's normal rating from scratch using the remaining `completedChallenges` and the current `challenges.json` ratings.
    2. **Do not change the player's rank**, even if the new rating falls below the `ratingNeeded` of their current rank. (Manual derank via `/derank` is required if needed.)
  - **Bonus challenge removed:**
    - Subtract the removed bonus challenge's rating from the player's bonus rating.
    - *(Bonus rating does not affect rank.)*
- Re‑sort the player lists:
  - If a normal challenge was removed, re‑sort the global `players` array and the affected rank's `players` array by normal rating descending.
  - If a bonus challenge was removed, no re‑sort is necessary (player order depends only on normal rating).
- Reply with a confirmation message (e.g., "Normal challenge #X removed from @player. New normal rating: Y." or "Bonus challenge #X removed. New bonus rating: Z.").

#### Behavior notes

- **If the player is not in `players.json`** → ephemeral error: "Player not found."
- **If the challenge number is not in the player's completed list** → ephemeral error specifying whether it was a normal or bonus challenge.
- **Removing a normal challenge only updates the rating, not the rank.**
  If the player's rating drops below the threshold for their current rank, they keep the rank until an admin uses `/derank`.
- **Removing a bonus challenge does not affect rank or normal rating.**
- **If the player is the Air Dribble God and their normal rating decreases**, they may lose the title if another player now has a higher rating.
  Update `airdribblegod.player` to the new top player's Discord ID and set `airdribblegod.ratingNeeded` to that player's rating.
  **Discord roles must be updated:** remove the Air Dribble God role from the old holder and assign it to the new one.

## User commands

### `/check-ranking`

Show player rankings with optional filters.


| Parameter | Type      | Required | Description                                            |
| --------- | --------- | -------- | ------------------------------------------------------ |
| (none)    | -         | -        | Show the normal ranking.                               |
| `bonus`   | `boolean` | ❌       | Show the bonus ranking.                                |
| `rank`    | `string`  | ❌       | Filter by rank name (for example,`"Air Dribble God"`). |

#### Functionality

- Load all players from `players.json`.
- If **no parameters** are provided:
  - Display the normal ranking.
  - Players are already sorted by normal rating descending (highest first).
- If **`bonus` is `true`**:
  - Sort a copy of the player list by `bonusRating` descending.
  - Display the bonus ranking.
  - *If `rank` is also provided, it is ignored – bonus takes precedence.*
- If **only `rank` is provided** (and `bonus` is `false`/omitted):
  - Filter players to those whose `rank` field matches the chosen rank.
  - The filtered list preserves the global normal rating order.
- **Pagination:**
  - The list is split into pages of **10 players**.
  - Navigation buttons allow moving between pages: `⏮️ First`, `◀️ Previous`, `▶️ Next`, `⏭️ Last`.
  - The current page number and total pages are shown.
- **Display format:**
  - The first three positions show medals: 🏆 1st, 🥈 2nd, 🥉 3rd.
  - Subsequent positions show their numeric rank.
  - Each entry shows the player mention and their points (normal rating or bonus rating, depending on the mode).
  - If a rank filter is active, the rank name is included in the title (e.g., "🏆 Air Dribble Rating (Beginner)").

#### Behavior notes

- **If no players exist at all** → the leaderboard displays "No players yet."
- **If a rank filter is used and no player has that rank** → displays "No players in the **[Rank Name]** rank."
- **If `bonus` is `true` and `rank` is also provided** → the rank filter is ignored; the bonus ranking is shown.
- **The pagination buttons only work on the most recent `/check-ranking` message.**
  Using buttons on an older message will show an ephemeral error asking to run the command again.

---

### `/challenge-list`

Paginated list of challenges.


| Parameter | Type      | Required | Description                                         |
| --------- | --------- | -------- | --------------------------------------------------- |
| (none)    | -         | -        | Show all the normal challenges.                     |
| `bonus`   | `boolean` | ❌       | Lists the bonus challenges.                         |
| `rank`    | `string`  | ❌       | Filter challenges by rank.                          |
| `number`  | `number`  | ❌       | Show all the information from a specific challenge. |

#### Functionality

- **If a `number` is provided:**

  - Look up a single challenge:
    - If `bonus` is `true` → search in `bonus.json`.
    - Otherwise → search in `challenges.json`.
  - **If not found** → reply with an ephemeral error: "The [normal/bonus] challenge with number X does not exist."
  - If found, display a detailed view containing:
    - The formatted challenge number (e.g., `001`, `002`, … for normals; just the number for bonus).
    - Challenge name.
    - Type (normal / bonus).
    - Rank name (for normal challenges).
    - Rating.
    - Description.
    - Up to two example URLs.
- **If no `number` is provided (paginated list):**

  - Load the relevant challenge list:
    - **Bonus challenges** → from `bonus.json`.
      *(The `rank` parameter is ignored in this mode.)*
    - **Normal challenges** → from `challenges.json`, optionally filtered by `rank` if provided.
  - **Pagination:**
    - Challenges are split into pages of **8**.
    - Navigation buttons allow moving: `⏮️ First`, `◀️ Previous`, `▶️ Next`, `⏭️ Last`.
    - The current page and total pages are displayed (e.g., "Page 1/3").
  - **Display format:**
    - **Bonus challenges:**
      `**N) Name**`
      `rating rating | Example: link1 (| link2)`
      `description`
    - **Normal challenges:**
      The number is zero‑padded to three digits (e.g., `001`, `012`, `101`).
      `**NNN) Name**`
      `Rank | rating rating | Example: link1 (| link2)`
      `description`
    - If the list is empty, show "No challenges available".
  - **Title bar:**
    - For bonus or unfiltered normal: `"📋 All [normal/bonus] challenges"`.
    - For normal filtered by rank:
      - Title shows the rank name and the rating range for that rank.The range is calculated as:
        - Lower bound: `ratingNeeded` of the previous rank ÷ 1.1.
        - Upper bound:
          - For all ranks except Air Dribble God: `ratingNeeded` of the current rank ÷ 1.1.
          - For Air Dribble God: lower bound + sum of all Air Dribble God challenge ratings.

#### Behavior notes

- **If a specific `number` is requested but not found** → ephemeral error.
- **`bonus` takes precedence over `rank`** – if both are provided, the bonus list is shown (rank filter is ignored).
- **Pagination buttons only work on the most recent message** for that category (normal, bonus, or rank‑filtered).
  Using buttons on an older message results in an ephemeral error asking to run the command again.
- **Zero‑padding for normal challenge numbers** is purely cosmetic (e.g., `1` → `001`, `12` → `012`).
  Bonus numbers are displayed as plain integers.

---

### `/submit`

Submit a completed challenge.


| Parameter | Type      | Required | Description                            |
| --------- | --------- | -------- | -------------------------------------- |
| `bonus`   | `boolean` | ✅       | Submit for bonus or normal challenge.  |
| `number`  | `number`  | ✅       | The number of the challenge to submit. |
| `url`     | `string`  | ✅       | Url of the video.                      |
| `notes`   | `string`  | ❌       | Notes for moderators.                  |

#### Functionality

**1. Player submission**

- Look up the challenge by `number`:
  - If `bonus` is `true` → search in `bonus.json`.
  - If `bonus` is `false` → search in `challenges.json`.
- **If the challenge does not exist** → reply with an ephemeral error: "Challenge not found. Please check the number and try again."
- Verify that a moderator channel has been configured (via `/set-settings`):
  - If no channel is set → ephemeral error: "❌ There is not any channel configured. Use /set-channel"
  - If the channel no longer exists or is not text‑based → ephemeral error explaining the problem.
- Load the player from `players.json`:
  - **If the player is not yet registered:**
    - Add a new player object with:
      - `id`: Discord user ID
      - `rank`: `"grounddweller"`
      - `rating`: `0`
      - `bonusRating`: `0`
      - `completedChallenges`: `[]`
      - `completedBonus`: `[]`
      - `challengesWaitingToBeRated`: (if normal) `[{ messageId: null, number, url, accepted: null }]`
      - `bonusWaitingToBeRated`: (if bonus) `[{ messageId: null, number, url, accepted: null }]`
    - Add the player's ID to the `players` array of the `grounddweller` rank in `ranks.json`.
  - **If the player already exists:**
    - Reject the submission if the player **already completed** this challenge (normal or bonus) → ephemeral error: "❌ You have already completed this challenge."
    - Reject if the player already has this exact challenge **pending review** → ephemeral error: "❌ You have already submitted this challenge. Wait for the submission to be accepted."
    - Otherwise, append `{ messageId: null, number, url, accepted: null }` to `challengesWaitingToBeRated` (normal) or `bonusWaitingToBeRated` (bonus).
- Prepare an embed with the submission details and send it to the configured moderator channel, along with two buttons: **Accept ✅** and **Decline ❌**.
- Store the Discord message ID of that moderator message in the player's waiting list entry (`messageId` field).
- Save all modified JSON files.
- Reply to the player with a confirmation embed: "✅ Submission received … pending review."

**2. Moderator review (button interaction)**

- Only users with the `Manage Messages` permission can use the buttons.
- When a moderator clicks **Accept** or **Decline**:
  - The bot locates the player by searching for the `messageId` in all `challengesWaitingToBeRated` and `bonusWaitingToBeRated` arrays.
  - If the submission data is missing (e.g., challenge deleted), the buttons are disabled, an ephemeral error is shown, and the message is automatically deleted after a countdown.
  - Otherwise, the submission's `accepted` field is set to `true` (Accept) or `false` (Decline), and a modal appears asking for an **optional reason**.
- When the moderator submits the modal:
  - **If accepted:**
    - Move the challenge from the waiting list to the completed list:
      - Normal → `completedChallenges`
      - Bonus → `completedBonus`
    - **Normal challenge accepted:**
      - Recalculate **every player's** normal rating and rank (because a new Air Dribble God may emerge).
      - Update all Discord roles accordingly.
      - Re‑sort players inside each rank and globally by normal rating descending.
    - **Bonus challenge accepted:**
      - Recalculate only that player's bonus rating (via `updatePlayerBonusRating`).
      - No rank or role changes are triggered.
  - **If declined:**
    - The challenge is simply removed from the waiting list. No rating or rank changes occur.
  - The original moderator message has its buttons disabled.
  - A DM is sent to the player with the result (accept/decline), the reason, and a button to delete the DM.
  - If the DM cannot be delivered (e.g., DMs closed), an ephemeral warning is shown to the moderator.
  - The moderator's follow‑up message self‑destructs after a 5‑second countdown.

#### Behavior notes

- **URL validation:** If the provided URL does not start with `http://` or `https://`, the bot automatically prepends `https://`.
- **Duplicate prevention:** A player cannot submit a challenge they have already completed or that is already awaiting review.
- **New players** are automatically registered with the `grounddweller` rank when they submit their first challenge.
- **Accepting a normal challenge triggers a full ranking update** for all players, which may change the Air Dribble God title and multiple roles. Accepting a bonus challenge only updates that player's bonus rating.
- **Moderator messages self‑destruct** after the review is processed to keep the channel clean.
- **If the moderator lacks `Manage Messages`**, the buttons are ignored with an ephemeral error.
- **All file writes (`players.json`, `ranks.json`) are performed after every state change** to ensure data consistency.

---

### `/profile-stats`

See someone's profile. It shows name, rank, position in the normal and bonus leaderboard and rating on both.


| Parameter | Type   | Required | Description                            |
| --------- | ------ | -------- | -------------------------------------- |
| (none)    | -      | -        | Show own stats.                        |
| `player`  | `user` | ❌       | The player whose stats will be showed. |

#### Functionality

- Determine the target user:
  - If `player` is provided → use that user's ID.
  - Otherwise → use the ID of the user who ran the command.
- Look up the player in `players.json` by their Discord ID.
- **If the player is not found** → reply with an ephemeral message: "The user <@ID> has not completed any challenges yet."
- Calculate the player's **position** in both leaderboards:
  - **Normal leaderboard:** the `players` array is already sorted by `rating` descending → the position is the index + 1.
  - **Bonus leaderboard:** a copy of the `players` array is sorted by `bonusRating` descending → the position is the index + 1 in that sorted copy.
- Retrieve the player's **avatar** (for display):
  - If the target is the command user → use their cached avatar.
  - Otherwise → attempt to fetch the target user's avatar from Discord.
  - If fetching fails → proceed without an avatar (no error shown).
- Build the profile card:
  - **Title:** "📊 Stats for @Player"
  - **Rank section:** shows the rank name (from `ranks.json`) and, if available, the player's avatar as a thumbnail.
  - **Leaderboard section:**
    - 🏆 **Air Dribble Leaderboard**
      Position: `Xth` (with ordinal suffix)
      Rating: `player.rating`
    - 🎁 **Bonus Leaderboard**
      Position: `Xth`
      Rating: `player.bonusRating`
- Reply with the profile card (no user mentions are pinged).

#### Behavior notes

- **If the player is not yet in `players.json`** → ephemeral message: "The user <@ID> has not completed any challenges yet."
- **Position suffixes** use English ordinals (`1st`, `2nd`, `3rd`, `4th`, …; special handling for 11th, 12th, 13th).
- **Avatar fetching** may fail for users not in cache (e.g., left the server); in that case the profile is still shown, just without a thumbnail.
- **No data is modified** – this is a read‑only command.
- **Bonus leaderboard positions are computed live** (the `players` array is not permanently sorted by bonus rating).

---

### `/profile-challenges`

See someone's completed challenges.


| Parameter | Type      | Required | Description                                           |
| --------- | --------- | -------- | ----------------------------------------------------- |
| (none)    | -         | -        | Show own completed challenges.                        |
| `player`  | `user`    | ❌       | The player whose completed challenges will be showed. |
| `bonus`   | `boolean` | ❌       | Switch between bonus and normal challenges            |

#### Functionality

- Determine the target user:
  - If `player` is provided → use that user's ID.
  - Otherwise → use the ID of the user who ran the command.
- Look up the player in `players.json` by their Discord ID.
- **If the player is not found** → reply with an ephemeral message: "The user <@ID> has not completed any challenge yet."
- Select the challenge list to display:
  - If `bonus` is `true` → use the player's `completedBonus` array.
  - Otherwise → use `completedChallenges`.
- **Pagination:**
  - The list is split into pages of **10** challenges.
  - Navigation buttons allow moving: `⏮️ First`, `◀️ Previous`, `▶️ Next`, `⏭️ Last`.
  - Current page and total pages are shown (e.g., "Page 1/3").
- **Display format:**
  - **Header:** shows the player mention and their current rank name.
  - For each completed challenge:
    - **Normal challenges:** the number is zero‑padded to three digits (e.g., `001`, `012`, `101`), followed by the challenge name (from `challenges.json`), its rating, and a clickable link to the submitted video.
    - **Bonus challenges:** the number is shown as a plain integer, with the name (from `bonus.json`), rating, and video link.
  - If the selected list is empty → displays: "There are no [normal/bonus] completed challenges yet."
- **Read‑only:** no data is modified.

#### Behavior notes

- **If the player is not yet in `players.json`** → ephemeral message: "The user <@ID> has not completed any challenge yet."
- **Zero‑padding** applies only to normal challenges; bonus numbers are displayed as plain integers.
- **Pagination buttons** only work on the most recent `/profile-challenges` message. Using older buttons results in an ephemeral error.
- **Player mentions are not pinged** (the reply uses `allowedMentions: { parse: [] }`).
- **Challenge names and ratings** are always looked up from the current `challenges.json` / `bonus.json` files, so the display reflects the latest data even if challenges were later edited.
