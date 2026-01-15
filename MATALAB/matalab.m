
% Smart Home Monitoring → Telegram Alerts (HTML)
% ThingSpeak MATLAB Analysis (NO persistent variables)
% - Reads sensors from Main Channel
% - Classifies OK/WARNING/DANGER
% - Sends Telegram HTML notification with all sensor readings
% - Uses Alert Channel to store last state + last send time (epoch)

MAIN_CH  = 3186331;
READ_KEY = 'T5ZHZWFMJSOI964G';  % main channel read key
% CONFIG: ALERT CHANNEL (state + time storage)
ALERT_CH        = 3228263;
ALERT_WRITE_KEY = 'RNMN386IDZL45N67';

% Alert channel usage:
% field1 = lastStateCode (0 OK, 1 WARNING, 2 DANGER)
% field2 = lastStateText (optional)
% field3 = lastSentEpoch (unix seconds)

% CONFIG: TELEGRAM

BOT_TOKEN = '8183418202:AAH8HMEAxsUS2d4xQm3jRPvoHOXAX6hwyiU';
CHAT_ID   = '8186299690';

TELEGRAM_ENABLED = true;
% Rate limit seconds by state
MIN_GAP_OK      = 180;   % OK: at most every 3 min
MIN_GAP_WARNING = 60;    % WARNING: at most every 1 min
MIN_GAP_DANGER  = 30;    % DANGER: at most every 30 sec

% READ LATEST SENSOR VALUES

gas    = thingSpeakRead(MAIN_CH,'Fields',1,'NumPoints',1,'ReadKey',READ_KEY); gas = gas(end);
flame  = thingSpeakRead(MAIN_CH,'Fields',2,'NumPoints',1,'ReadKey',READ_KEY); flame = flame(end);
temp   = thingSpeakRead(MAIN_CH,'Fields',3,'NumPoints',1,'ReadKey',READ_KEY); temp = temp(end);
hum    = thingSpeakRead(MAIN_CH,'Fields',4,'NumPoints',1,'ReadKey',READ_KEY); hum = hum(end);
motion = thingSpeakRead(MAIN_CH,'Fields',5,'NumPoints',1,'ReadKey',READ_KEY); motion = motion(end);
rain   = thingSpeakRead(MAIN_CH,'Fields',6,'NumPoints',1,'ReadKey',READ_KEY); rain = rain(end);

% Clean NaNs (defensive)
if isnan(gas), gas = 0; end
if isnan(flame), flame = 0; end
if isnan(temp), temp = -1; end
if isnan(hum), hum = -1; end
if isnan(motion), motion = 0; end
if isnan(rain), rain = 0; end

disp("Latest readings -> Gas=" + gas + " Flame=" + flame + " Temp=" + temp + " Hum=" + hum + " Motion=" + motion + " Rain=" + rain);

% CLASSIFY STATE (edit thresholds as you want)

stateCode = 0; stateText = "OK"; icon = "✅";

% Example rules:
% DANGER: flame==1 OR gas>=400
% WARNING: gas>=170 OR motion==1
if flame == 1 || gas >= 400
    stateCode = 2; stateText = "DANGER"; icon = "????";
elseif gas >= 170 || motion == 1
    stateCode = 1; stateText = "WARNING"; icon = "⚠️";
end

% READ PREVIOUS STATE + TIME FROM ALERT CHANNEL

prev = thingSpeakRead(ALERT_CH,'NumPoints',1,'OutputFormat','table');
prevState = safeGetField(prev, 1);   % lastStateCode
prevEpoch = safeGetField(prev, 3);   % lastSentEpoch
if isnan(prevState), prevState = -1; end
if isnan(prevEpoch), prevEpoch = 0; end
nowEpoch = posixtime(datetime("now"));
% Pick rate limit based on current state
minGap = MIN_GAP_OK;
if stateCode == 1, minGap = MIN_GAP_WARNING; end
if stateCode == 2, minGap = MIN_GAP_DANGER; end

% Send decision: state changed OR time gap reached
shouldSend = (stateCode ~= prevState) || ((nowEpoch - prevEpoch) >= minGap);

if ~shouldSend
    disp("Telegram skipped (no change / rate limited).");
    return;
end

% BUILD TELEGRAM HTML MESSAGE (parse_mode=HTML)
timeStr = datestr(now, 'yyyy-mm-dd HH:MM:SS');
% Format values nicely
gasStr    = num2str(gas, '%.0f');
flameStr  = num2str(flame, '%.0f');
tempStr   = num2str(temp, '%.2f');
humStr    = num2str(hum, '%.2f');
motionStr = num2str(motion, '%.0f');
rainStr   = num2str(rain, '%.0f');
msg = [
    "<b>???? Smart Home Monitoring</b>" newline ...
    "<b>Status:</b> " icon " <b>" char(stateText) "</b>" newline newline ...
    "<b>???? Sensor Readings</b>" newline ...
    "• <b>Gas:</b> <code>" gasStr "</code>" newline ...
    "• <b>Flame:</b> <code>" flameStr "</code>" newline ...
    "• <b>Temperature:</b> <code>" tempStr " °C</code>" newline ...
    "• <b>Humidity:</b> <code>" humStr " %</code>" newline ...
    "• <b>Motion:</b> <code>" motionStr "</code>" newline ...
    "• <b>Rain:</b> <code>" rainStr "</code>" newline newline ...
    "<i>Time:</i> <code>" timeStr "</code>" newline ...
    "<a href=""https://thingspeak.com/channels/3186331"">View ThingSpeak Dashboard</a>"
];
% SEND TELEGRAM (POST JSON)
if TELEGRAM_ENABLED
    url = "https://api.telegram.org/bot" + string(BOT_TOKEN) + "/sendMessage";
    body = struct();
    body.chat_id = char(CHAT_ID);
    body.text = char(msg);
    body.parse_mode = "HTML";
    body.disable_web_page_preview = true;

    try
        options = weboptions('MediaType','application/json','Timeout',10);
        resp = webwrite(url, body, options);

        if isstruct(resp) && isfield(resp,"ok") && resp.ok
            disp("Telegram sent: " + stateText);

            % Update Alert Channel: state code + state text + epoch
            thingSpeakWrite(ALERT_CH, [stateCode, string(stateText), nowEpoch], ...
                'Fields', [1,2,3], 'WriteKey', ALERT_WRITE_KEY);
        else
            disp("Telegram FAILED (response not ok):");
            disp(resp);
        end

    catch ME
        disp("Telegram ERROR (exception):");
        disp(ME.message);
    end
else
    disp("TELEGRAM_DISABLED. Not sending message.");
end

% HELPER FUNCTION (must be at bottom in ThingSpeak)

function val = safeGetField(T, idx)
% Safely get field idx (1..8) from ThingSpeak table output.
% Works for Field1/field1/custom naming.
    val = NaN;
    if isempty(T) || height(T)==0
        return;
    end

    names = T.Properties.VariableNames;

    c1 = sprintf("Field%d", idx);
    c2 = sprintf("field%d", idx);

    if any(strcmp(names, c1))
        val = T.(c1)(1);
        return;
    end
    if any(strcmp(names, c2))
        val = T.(c2)(1);
        return;
    end

    % Contains pattern fallback
    pat = sprintf("field%d", idx);
    hit = find(contains(lower(names), pat), 1);
    if ~isempty(hit)
        val = T.(names{hit})(1);
        return;
    end
    % Positional fallback: only columns that start with 'field'
    fieldCols = find(startsWith(lower(names), "field"));
    if numel(fieldCols) >= idx
        val = T.(names{fieldCols(idx)})(1);
    end
end
